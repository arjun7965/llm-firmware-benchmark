import type {
  AsyncCache,
  CacheLoader,
  SingleflightCacheOptions,
} from "../../starter/cache_api";
import type {
  CacheLoader as CandidateLoader,
  SingleflightCacheOptions as CandidateOptions,
} from "../../generated/answer";

declare const process: {
  exitCode?: number;
  on(event: "exit", listener: () => void): void;
};

type CacheConstructor = {
  new <K, V>(options: SingleflightCacheOptions): AsyncCache<K, V>;
};

interface Deferred<T> {
  promise: Promise<T>;
  resolve(value: T): void;
  reject(reason: unknown): void;
}

let completed = false;
process.on("exit", () => {
  if (!completed) process.exitCode = 1;
});

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function equal<T>(actual: T, expected: T, message: string): void {
  assert(Object.is(actual, expected), `${message}: ${String(actual)}`);
}

async function rejects(promise: Promise<unknown>, expected: unknown): Promise<void> {
  let rejected = false;
  try {
    await promise;
  } catch (error) {
    rejected = true;
    equal(error, expected, "rejection reason mismatch");
  }
  assert(rejected, "expected promise rejection");
}

function deferred<T>(): Deferred<T> {
  let resolve!: (value: T) => void;
  let reject!: (reason: unknown) => void;
  const promise = new Promise<T>((resolvePromise, rejectPromise) => {
    resolve = resolvePromise;
    reject = rejectPromise;
  });
  return { promise, reject, resolve };
}

class FakeClock {
  private value = 1_000;

  readonly now = (): number => this.value;

  advance(milliseconds: number): void {
    this.value += milliseconds;
  }
}

async function testTtlAndLazyExpiry(Cache: CacheConstructor): Promise<void> {
  const clock = new FakeClock();
  const cache = new Cache<string, number>({ ttlMs: 100, now: clock.now });
  let loads = 0;
  const loader = () => ++loads;

  equal(await cache.get("key", loader), 1, "first load");
  equal(await cache.get("key", loader), 1, "cached load");
  clock.advance(99);
  equal(await cache.get("key", loader), 1, "value before expiry");
  clock.advance(1);
  equal(await cache.get("key", loader), 2, "value at expiry");
  equal(loads, 2, "TTL loader count");

  const zeroTtl = new Cache<string, number>({ ttlMs: 0, now: clock.now });
  let zeroTtlLoads = 0;
  equal(await zeroTtl.get("key", () => ++zeroTtlLoads), 1, "zero TTL first load");
  equal(await zeroTtl.get("key", () => ++zeroTtlLoads), 2, "zero TTL reuse");
}

async function testCoalescing(Cache: CacheConstructor): Promise<void> {
  const cache = new Cache<string, number>({ ttlMs: 100 });
  const pending = deferred<number>();
  let loads = 0;
  const loader: CacheLoader<string, number> = (key) => {
    equal(key, "same", "loader key");
    loads++;
    return pending.promise;
  };

  const first = cache.get("same", loader);
  const second = cache.get("same", loader);
  equal(loads, 1, "coalesced loader count");
  pending.resolve(7);
  equal(await first, 7, "first coalesced value");
  equal(await second, 7, "second coalesced value");
}

async function testReentrantCoalescing(Cache: CacheConstructor): Promise<void> {
  const cache = new Cache<string, number>({ ttlMs: 100 });
  let nested: Promise<number> | undefined;
  let loads = 0;
  const outer = cache.get("key", (key) => {
    loads++;
    nested = cache.get(key, () => {
      loads++;
      return 99;
    });
    return 42;
  });

  equal(await outer, 42, "outer reentrant value");
  assert(nested !== undefined, "nested get was not called");
  equal(await nested, 42, "nested caller did not join current flight");
  equal(loads, 1, "reentrant loader count");
}

async function testCallerLocalCancellation(Cache: CacheConstructor): Promise<void> {
  const cache = new Cache<string, number>({ ttlMs: 100 });
  const pending = deferred<number>();
  const controller = new AbortController();
  const reason = new Error("caller canceled");
  let loads = 0;
  const loader = () => {
    loads++;
    return pending.promise;
  };

  const canceled = cache.get("key", loader, controller.signal);
  const retained = cache.get("key", loader);
  controller.abort(reason);
  const alreadyAborted = new AbortController();
  const earlyReason = new Error("already canceled");
  alreadyAborted.abort(earlyReason);
  await rejects(cache.get("key", loader, alreadyAborted.signal), earlyReason);
  equal(loads, 1, "aborted caller joined current flight");
  pending.resolve(9);
  await rejects(canceled, reason);
  equal(loads, 1, "cancellation started another loader");
  equal(await retained, 9, "shared load was canceled");

  await rejects(cache.get("other", loader, alreadyAborted.signal), earlyReason);
  equal(loads, 1, "aborted caller invoked loader");
}

async function testFailuresAreNotCached(Cache: CacheConstructor): Promise<void> {
  const cache = new Cache<string, number>({ ttlMs: 100 });
  const asynchronous = new Error("asynchronous failure");
  await rejects(cache.get("async", () => Promise.reject(asynchronous)), asynchronous);
  equal(await cache.get("async", () => 11), 11, "rejection was cached");

  const synchronous = new Error("synchronous failure");
  const failed = cache.get("sync", () => {
    throw synchronous;
  });
  await rejects(failed, synchronous);
  equal(await cache.get("sync", () => 12), 12, "synchronous throw was cached");
}

async function testInvalidateFencesOldFlight(Cache: CacheConstructor): Promise<void> {
  const cache = new Cache<string, string>({ ttlMs: 1_000 });
  const oldLoad = deferred<string>();
  const freshLoad = deferred<string>();
  let loads = 0;
  const loader = () => (++loads === 1 ? oldLoad.promise : freshLoad.promise);

  const oldCaller = cache.get("key", loader);
  const invalidateResult: unknown = cache.invalidate("key");
  equal(invalidateResult, undefined, "invalidate return value");
  const freshCaller = cache.get("key", loader);
  equal(loads, 2, "invalidate reused stale flight");
  freshLoad.resolve("fresh");
  equal(await freshCaller, "fresh", "fresh load result");
  oldLoad.resolve("stale");
  equal(await oldCaller, "stale", "old caller result");
  equal(await cache.get("key", loader), "fresh", "stale flight repopulated cache");

  cache.invalidate("key");
  equal(await cache.get("key", () => "new"), "new", "cached value survived invalidate");
}

async function testClearFencesEveryFlight(Cache: CacheConstructor): Promise<void> {
  const cache = new Cache<string, string>({ ttlMs: 1_000 });
  const oldA = deferred<string>();
  const oldB = deferred<string>();
  const oldCallerA = cache.get("a", () => oldA.promise);
  const oldCallerB = cache.get("b", () => oldB.promise);
  const clearResult: unknown = cache.clear();
  equal(clearResult, undefined, "clear return value");

  const freshCallerA = cache.get("a", () => "fresh-a");
  const freshCallerB = cache.get("b", () => "fresh-b");
  oldA.resolve("stale-a");
  oldB.resolve("stale-b");
  equal(await freshCallerA, "fresh-a", "clear did not reload a");
  equal(await freshCallerB, "fresh-b", "clear did not reload b");
  await Promise.all([oldCallerA, oldCallerB]);
  equal(await cache.get("a", () => "wrong"), "fresh-a", "old a repopulated cache");
  equal(await cache.get("b", () => "wrong"), "fresh-b", "old b repopulated cache");

  cache.clear();
  equal(await cache.get("a", () => "after-clear"), "after-clear", "clear kept value");
}

async function testNoEntryTimers(Cache: CacheConstructor): Promise<void> {
  const originalSetTimeout = globalThis.setTimeout;
  const originalSetInterval = globalThis.setInterval;
  let timerCalls = 0;
  const rejectTimer = (..._arguments: unknown[]) => {
    timerCalls++;
    throw new Error("per-entry timer created");
  };
  globalThis.setTimeout = rejectTimer as typeof setTimeout;
  globalThis.setInterval = rejectTimer as typeof setInterval;
  try {
    const cache = new Cache<string, number>({ ttlMs: 100 });
    equal(await cache.get("key", () => 1), 1, "timer test load");
    equal(await cache.get("key", () => 2), 1, "timer test cache hit");
  } finally {
    globalThis.setTimeout = originalSetTimeout;
    globalThis.setInterval = originalSetInterval;
  }
  equal(timerCalls, 0, "timer call count");
}

function testOptions(Cache: CacheConstructor): void {
  for (const ttlMs of [-1, Number.NaN, Number.POSITIVE_INFINITY]) {
    let error: unknown;
    try {
      new Cache({ ttlMs });
    } catch (caught) {
      error = caught;
    }
    assert(error instanceof RangeError, `accepted invalid ttlMs ${ttlMs}`);
  }
}

async function main(): Promise<void> {
  const optionContract: CandidateOptions = { ttlMs: 1 };
  const loaderContract: CandidateLoader<string, number> = (key) => key.length;
  void optionContract;
  void loaderContract;
  const candidate: typeof import("../../generated/answer") =
    await import("../../generated/answer");
  const Cache: CacheConstructor = candidate.SingleflightCache;

  testOptions(Cache);
  await testTtlAndLazyExpiry(Cache);
  await testCoalescing(Cache);
  await testReentrantCoalescing(Cache);
  await testCallerLocalCancellation(Cache);
  await testFailuresAreNotCached(Cache);
  await testInvalidateFencesOldFlight(Cache);
  await testClearFencesEveryFlight(Cache);
  await testNoEntryTimers(Cache);
  completed = true;
  console.log("TypeScript singleflight-cache public tests passed.");
}

void main().catch((error: unknown) => {
  console.error(error);
  process.exitCode = 1;
});
