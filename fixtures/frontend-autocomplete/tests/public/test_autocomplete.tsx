import type {
  AutocompleteComponent,
  SearchUsers,
  User,
} from "../../starter/autocomplete_api";

declare const require: (name: string) => unknown;
declare const process: {
  exitCode?: number;
  on(event: "exit", listener: () => void): void;
};

interface Deferred<T> {
  promise: Promise<T>;
  resolve(value: T): void;
  reject(reason: unknown): void;
}

interface TimerJob {
  callback: (...args: unknown[]) => void;
  args: unknown[];
  due: number;
  id: number;
}

let Candidate: AutocompleteComponent;

const { JSDOM } = require("jsdom") as {
  JSDOM: new (html: string, options: { url: string }) => {
    window: Window & typeof globalThis & { close(): void };
  };
};

const dom = new JSDOM("<!doctype html><html><body></body></html>", {
  url: "https://benchmark.invalid/",
});

for (const name of [
  "document",
  "Element",
  "Event",
  "HTMLElement",
  "HTMLInputElement",
  "KeyboardEvent",
  "MouseEvent",
  "MutationObserver",
  "Node",
  "navigator",
] as const) {
  Object.defineProperty(globalThis, name, {
    configurable: true,
    value: dom.window[name],
  });
}
Object.defineProperty(globalThis, "window", {
  configurable: true,
  value: dom.window,
});
Object.defineProperty(globalThis, "getComputedStyle", {
  configurable: true,
  value: dom.window.getComputedStyle.bind(dom.window),
});
(globalThis as unknown as { IS_REACT_ACT_ENVIRONMENT: boolean })
  .IS_REACT_ACT_ENVIRONMENT = true;

const {
  act,
  cleanup,
  fireEvent,
  render,
} = require("@testing-library/react") as
  typeof import("@testing-library/react");

let completed = false;
process.on("exit", () => {
  if (!completed) process.exitCode = 1;
});

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(message);
}

function equal<T>(actual: T, expected: T, message: string): void {
  assert(Object.is(actual, expected),
    `${message}: expected ${String(expected)}, received ${String(actual)}`);
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

class FakeTimers {
  private readonly jobs = new Map<number, TimerJob>();
  private nextId = 1;
  private now = 0;
  private readonly originalSetTimeout = globalThis.setTimeout;
  private readonly originalClearTimeout = globalThis.clearTimeout;

  install(): void {
    globalThis.setTimeout = this.setTimeout as typeof globalThis.setTimeout;
    globalThis.clearTimeout =
      this.clearTimeout as typeof globalThis.clearTimeout;
  }

  restore(): void {
    this.jobs.clear();
    globalThis.setTimeout = this.originalSetTimeout;
    globalThis.clearTimeout = this.originalClearTimeout;
  }

  advanceBy(milliseconds: number): void {
    const target = this.now + milliseconds;
    while (true) {
      const next = [...this.jobs.values()]
        .filter((job) => job.due <= target)
        .sort((left, right) => left.due - right.due || left.id - right.id)[0];
      if (!next) break;
      this.jobs.delete(next.id);
      this.now = next.due;
      next.callback(...next.args);
    }
    this.now = target;
  }

  private readonly setTimeout = (
    callback: (...args: unknown[]) => void,
    delay = 0,
    ...args: unknown[]
  ): number => {
    const id = this.nextId++;
    this.jobs.set(id, {
      callback,
      args,
      due: this.now + Math.max(0, Number(delay) || 0),
      id,
    });
    return id;
  };

  private readonly clearTimeout = (id: unknown): void => {
    this.jobs.delete(Number(id));
  };
}

async function settle(action: () => void): Promise<void> {
  await act(async () => {
    action();
    await Promise.resolve();
    await Promise.resolve();
  });
}

async function withTimers(
  body: (clock: FakeTimers) => Promise<void>,
): Promise<void> {
  const clock = new FakeTimers();
  clock.install();
  try {
    await body(clock);
  } finally {
    cleanup();
    clock.restore();
  }
}

function inputFrom(view: ReturnType<typeof render>): HTMLInputElement {
  const input = view.getByRole("combobox");
  assert(input instanceof HTMLInputElement, "combobox is not an input");
  return input;
}

async function testDebounceAndReset(): Promise<void> {
  await withTimers(async (clock) => {
    const pending = deferred<User[]>();
    const calls: Array<{ query: string; signal: AbortSignal }> = [];
    const searchUsers: SearchUsers = (query, signal) => {
      calls.push({ query, signal });
      return pending.promise;
    };
    const view = render(<Candidate searchUsers={searchUsers} />);
    const input = inputFrom(view);

    equal(input.value, "", "initial controlled value");
    equal(input.getAttribute("aria-expanded"), "false", "initial expansion");
    equal(input.getAttribute("aria-autocomplete"), "list", "autocomplete ARIA");
    assert(Boolean(input.getAttribute("aria-controls")), "missing aria-controls");

    fireEvent.change(input, { target: { value: "a" } });
    act(() => clock.advanceBy(249));
    equal(calls.length, 0, "request ran before 250 ms");
    fireEvent.change(input, { target: { value: "al" } });
    act(() => clock.advanceBy(1));
    equal(calls.length, 0, "obsolete debounce was not cleared");
    act(() => clock.advanceBy(249));
    equal(calls.length, 1, "request did not run at 250 ms");
    equal(calls[0].query, "al", "debounced query");
    assert(view.getByRole("status").textContent?.includes("Loading"),
      "loading feedback is not accessible");

    await settle(() => pending.resolve([{ id: "1", name: "Alan" }]));
    equal(view.getAllByRole("option").length, 1, "result count");

    fireEvent.change(input, { target: { value: "" } });
    act(() => clock.advanceBy(500));
    equal(calls.length, 1, "empty input started a search");
    equal(input.getAttribute("aria-expanded"), "false", "empty popup state");
    assert(view.queryByRole("listbox") === null, "results survived empty input");
  });
}

async function testCancellationAndStaleFence(): Promise<void> {
  await withTimers(async (clock) => {
    const oldRequest = deferred<User[]>();
    const newRequest = deferred<User[]>();
    const calls: Array<{ query: string; signal: AbortSignal }> = [];
    const searchUsers: SearchUsers = (query, signal) => {
      calls.push({ query, signal });
      return calls.length === 1 ? oldRequest.promise : newRequest.promise;
    };
    const view = render(<Candidate searchUsers={searchUsers} />);
    const input = inputFrom(view);

    fireEvent.change(input, { target: { value: "old" } });
    act(() => clock.advanceBy(250));
    equal(calls.length, 1, "old request count");
    fireEvent.change(input, { target: { value: "new" } });
    equal(calls[0].signal.aborted, true, "obsolete request was not aborted");
    act(() => clock.advanceBy(250));
    equal(calls.length, 2, "new request count");

    await settle(() => oldRequest.resolve([{ id: "old", name: "Old" }]));
    assert(view.queryByText("Old") === null, "stale response replaced state");
    assert(view.getByRole("status").textContent?.includes("Loading"),
      "stale response removed newer loading state");

    await settle(() => newRequest.resolve([{ id: "new", name: "New" }]));
    assert(view.getByText("New"), "new result missing");
    assert(view.queryByText("Old") === null, "stale result remained visible");

    fireEvent.change(input, { target: { value: "unmount" } });
    act(() => clock.advanceBy(250));
    equal(calls.length, 3, "unmount request count");
    view.unmount();
    equal(calls[2].signal.aborted, true, "unmount did not abort request");
  });
}

async function testKeyboardMouseAndAria(): Promise<void> {
  await withTimers(async (clock) => {
    const users = [
      { id: "ada", name: "Ada" },
      { id: "grace", name: "Grace" },
    ];
    let calls = 0;
    const searchUsers: SearchUsers = async () => {
      calls++;
      return users;
    };
    const view = render(<Candidate searchUsers={searchUsers} />);
    const input = inputFrom(view);

    fireEvent.change(input, { target: { value: "a" } });
    await settle(() => clock.advanceBy(250));
    const listbox = view.getByRole("listbox");
    equal(input.getAttribute("aria-controls"), listbox.id, "combobox control ID");
    equal(input.getAttribute("aria-expanded"), "true", "result expansion");
    let options = view.getAllByRole("option");
    equal(options[0].getAttribute("aria-selected"), "false", "initial option");

    fireEvent.keyDown(input, { key: "ArrowDown" });
    options = view.getAllByRole("option");
    equal(options[0].getAttribute("aria-selected"), "true", "ArrowDown choice");
    equal(input.getAttribute("aria-activedescendant"), options[0].id,
      "active descendant after ArrowDown");
    fireEvent.keyDown(input, { key: "ArrowUp" });
    options = view.getAllByRole("option");
    equal(options[1].getAttribute("aria-selected"), "true", "ArrowUp wrapping");
    fireEvent.keyDown(input, { key: "ArrowDown" });
    fireEvent.keyDown(input, { key: "Enter" });
    equal(input.value, "Ada", "Enter selection value");
    equal(input.getAttribute("aria-expanded"), "false", "Enter dismissal");
    assert(view.queryByRole("listbox") === null, "Enter kept listbox open");
    act(() => clock.advanceBy(250));
    equal(calls, 1, "selection started another search");

    fireEvent.change(input, { target: { value: "g" } });
    await settle(() => clock.advanceBy(250));
    fireEvent.keyDown(input, { key: "Escape" });
    equal(input.value, "g", "Escape changed query");
    equal(input.getAttribute("aria-expanded"), "false", "Escape dismissal");
    assert(view.queryByRole("listbox") === null, "Escape kept listbox open");

    fireEvent.change(input, { target: { value: "gr" } });
    await settle(() => clock.advanceBy(250));
    const grace = view.getByText("Grace");
    fireEvent.mouseDown(grace);
    fireEvent.click(grace);
    equal(input.value, "Grace", "mouse selection value");
    equal(input.getAttribute("aria-expanded"), "false", "mouse dismissal");
    act(() => clock.advanceBy(250));
    equal(calls, 3, "mouse selection started another search");
  });
}

async function testSameNameSelectionDoesNotSkipNextEdit(): Promise<void> {
  await withTimers(async (clock) => {
    let calls = 0;
    const searchUsers: SearchUsers = async () => {
      calls++;
      return [{ id: "ada", name: "Ada" }];
    };
    const view = render(<Candidate searchUsers={searchUsers} />);
    const input = inputFrom(view);

    fireEvent.change(input, { target: { value: "Ada" } });
    await settle(() => clock.advanceBy(250));
    const ada = view.getByRole("option");
    fireEvent.mouseDown(ada);
    fireEvent.click(ada);
    act(() => clock.advanceBy(250));
    equal(calls, 1, "same-name selection started another search");

    fireEvent.change(input, { target: { value: "next" } });
    await settle(() => clock.advanceBy(250));
    equal(calls, 2, "same-name selection suppressed the next edit");
  });
}

async function testFeedbackAndClearCancellation(): Promise<void> {
  await withTimers(async (clock) => {
    const requests = [deferred<User[]>(), deferred<User[]>(), deferred<User[]>()];
    const signals: AbortSignal[] = [];
    const searchUsers: SearchUsers = (_query, signal) => {
      signals.push(signal);
      const request = requests[signals.length - 1];
      assert(request, "unexpected search call");
      return request.promise;
    };
    const view = render(<Candidate searchUsers={searchUsers} />);
    const input = inputFrom(view);

    fireEvent.change(input, { target: { value: "empty" } });
    act(() => clock.advanceBy(250));
    await settle(() => requests[0].resolve([]));
    assert(view.getByRole("status").textContent?.includes("No"),
      "empty feedback is not accessible");

    fireEvent.change(input, { target: { value: "error" } });
    act(() => clock.advanceBy(250));
    await settle(() => requests[1].reject(new Error("offline")));
    assert(Boolean(view.getByRole("alert").textContent),
      "error feedback is not accessible");

    fireEvent.change(input, { target: { value: "pending" } });
    act(() => clock.advanceBy(250));
    fireEvent.change(input, { target: { value: "" } });
    equal(signals[2].aborted, true, "clear did not abort request");
    equal(input.getAttribute("aria-expanded"), "false", "clear dismissal");
    await settle(() => requests[2].resolve([{ id: "late", name: "Late" }]));
    assert(view.queryByText("Late") === null, "cleared stale result appeared");
  });
}

async function main(): Promise<void> {
  const candidate: typeof import("../../generated/Autocomplete") =
    await import("../../generated/Autocomplete");
  Candidate = candidate.default;
  await testDebounceAndReset();
  await testCancellationAndStaleFence();
  await testKeyboardMouseAndAria();
  await testSameNameSelectionDoesNotSkipNextEdit();
  await testFeedbackAndClearCancellation();
  completed = true;
  dom.window.close();
  console.log("Frontend autocomplete public tests passed.");
}

void main().catch((error: unknown) => {
  console.error(error);
  process.exitCode = 1;
});
