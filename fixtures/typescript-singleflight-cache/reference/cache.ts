export type CacheLoader<K, V> = (key: K) => V | PromiseLike<V>;

export interface SingleflightCacheOptions {
  ttlMs: number;
  now?: () => number;
}

interface CacheEntry<V> {
  value: V;
  expiresAt: number;
}

interface Flight<V> {
  promise: Promise<V>;
}

function abortReason(signal: AbortSignal): unknown {
  return signal.reason ?? new DOMException("The operation was aborted", "AbortError");
}

export class SingleflightCache<K, V> {
  private readonly ttlMs: number;
  private readonly now: () => number;
  private readonly values = new Map<K, CacheEntry<V>>();
  private readonly flights = new Map<K, Flight<V>>();

  constructor(options: SingleflightCacheOptions) {
    if (!Number.isFinite(options.ttlMs) || options.ttlMs < 0) {
      throw new RangeError("ttlMs must be a finite nonnegative number");
    }
    if (options.now !== undefined && typeof options.now !== "function") {
      throw new TypeError("now must be a function");
    }
    this.ttlMs = options.ttlMs;
    this.now = options.now ?? Date.now;
  }

  get(
    key: K,
    loader: CacheLoader<K, V>,
    signal?: AbortSignal,
  ): Promise<V> {
    if (signal?.aborted) {
      return Promise.reject(abortReason(signal));
    }

    const cached = this.values.get(key);
    if (cached && this.now() < cached.expiresAt) {
      return Promise.resolve(cached.value);
    }
    if (cached) this.values.delete(key);

    const existingFlight = this.flights.get(key);
    if (existingFlight) {
      return this.waitFor(existingFlight.promise, signal);
    }

    let resolveShared!: (value: V | PromiseLike<V>) => void;
    let rejectShared!: (reason?: unknown) => void;
    const shared = new Promise<V>((resolve, reject) => {
      resolveShared = resolve;
      rejectShared = reject;
    });
    const flight: Flight<V> = { promise: shared };
    this.flights.set(key, flight);

    void shared.then(
      (value) => {
        if (this.flights.get(key) !== flight) return;
        this.flights.delete(key);
        this.values.set(key, {
          value,
          expiresAt: this.now() + this.ttlMs,
        });
      },
      () => {
        if (this.flights.get(key) === flight) {
          this.flights.delete(key);
        }
      },
    );

    try {
      resolveShared(loader(key));
    } catch (error) {
      rejectShared(error);
    }
    return this.waitFor(shared, signal);
  }

  invalidate(key: K): void {
    this.values.delete(key);
    this.flights.delete(key);
  }

  clear(): void {
    this.values.clear();
    this.flights.clear();
  }

  private waitFor(shared: Promise<V>, signal?: AbortSignal): Promise<V> {
    if (!signal) return shared;
    if (signal.aborted) return Promise.reject(abortReason(signal));

    return new Promise<V>((resolve, reject) => {
      const onAbort = () => reject(abortReason(signal));
      signal.addEventListener("abort", onAbort, { once: true });
      void shared.then(
        (value) => {
          signal.removeEventListener("abort", onAbort);
          resolve(value);
        },
        (error) => {
          signal.removeEventListener("abort", onAbort);
          reject(error);
        },
      );
    });
  }
}
