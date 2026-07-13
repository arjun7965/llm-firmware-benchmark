import {
  useEffect,
  useId,
  useRef,
  useState,
  type ChangeEvent,
  type KeyboardEvent,
} from "react";

export interface User {
  id: string;
  name: string;
}

export interface AutocompleteProps {
  searchUsers(
    query: string,
    signal: AbortSignal,
  ): Promise<User[]>;
}

type Phase = "idle" | "loading" | "results" | "error";

const debounceMilliseconds = 250;

export default function Autocomplete({
  searchUsers,
}: AutocompleteProps): JSX.Element {
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<User[]>([]);
  const [phase, setPhase] = useState<Phase>("idle");
  const [open, setOpen] = useState(false);
  const [activeIndex, setActiveIndex] = useState(-1);
  const generation = useRef(0);
  const timer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const controller = useRef<AbortController | null>(null);
  const selectedQuery = useRef<string | null>(null);
  const baseId = useId();
  const popupId = `${baseId}-popup`;

  function clearTimer(): void {
    if (timer.current !== null) {
      clearTimeout(timer.current);
      timer.current = null;
    }
  }

  function abortRequest(request: AbortController | null): void {
    request?.abort();
  }

  function cancelWork(): void {
    generation.current++;
    clearTimer();
    abortRequest(controller.current);
    controller.current = null;
  }

  useEffect(() => {
    cancelWork();
    setResults([]);
    setActiveIndex(-1);
    setOpen(false);

    const justSelected = selectedQuery.current;
    selectedQuery.current = null;
    if (justSelected === query) {
      setPhase("idle");
      return undefined;
    }
    if (query === "") {
      setPhase("idle");
      return undefined;
    }

    setPhase("idle");
    const currentGeneration = generation.current;
    let requestController: AbortController | null = null;
    timer.current = setTimeout(() => {
      timer.current = null;
      requestController = new AbortController();
      controller.current = requestController;
      setPhase("loading");
      setOpen(true);

      void searchUsers(query, requestController.signal).then(
        (users) => {
          if (
            requestController?.signal.aborted ||
            generation.current !== currentGeneration
          ) {
            return;
          }
          controller.current = null;
          setResults(users);
          setActiveIndex(-1);
          setPhase("results");
          setOpen(true);
        },
        () => {
          if (
            requestController?.signal.aborted ||
            generation.current !== currentGeneration
          ) {
            return;
          }
          controller.current = null;
          setResults([]);
          setPhase("error");
          setOpen(true);
        },
      );
    }, debounceMilliseconds);

    return () => {
      clearTimer();
      abortRequest(requestController);
      if (controller.current === requestController) {
        controller.current = null;
      }
    };
  }, [query, searchUsers]);

  function selectUser(user: User): void {
    cancelWork();
    selectedQuery.current = user.name;
    setQuery(user.name);
    setResults([]);
    setActiveIndex(-1);
    setPhase("idle");
    setOpen(false);
  }

  function handleChange(event: ChangeEvent<HTMLInputElement>): void {
    setQuery(event.currentTarget.value);
  }

  function handleKeyDown(event: KeyboardEvent<HTMLInputElement>): void {
    if (event.key === "Escape" && open) {
      event.preventDefault();
      cancelWork();
      setResults([]);
      setActiveIndex(-1);
      setPhase("idle");
      setOpen(false);
      return;
    }
    if (!open || results.length === 0) return;

    if (event.key === "ArrowDown") {
      event.preventDefault();
      setActiveIndex((index) => (index + 1) % results.length);
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      setActiveIndex((index) =>
        index < 0 ? results.length - 1 :
          (index - 1 + results.length) % results.length);
    } else if (event.key === "Enter" && activeIndex >= 0) {
      event.preventDefault();
      selectUser(results[activeIndex]);
    }
  }

  const activeDescendant = open && activeIndex >= 0
    ? `${baseId}-option-${activeIndex}`
    : undefined;

  return (
    <div>
      <label htmlFor={`${baseId}-input`}>Search users</label>
      <input
        id={`${baseId}-input`}
        type="text"
        role="combobox"
        aria-autocomplete="list"
        aria-controls={popupId}
        aria-expanded={open}
        aria-activedescendant={activeDescendant}
        autoComplete="off"
        value={query}
        onChange={handleChange}
        onKeyDown={handleKeyDown}
      />

      {open && phase === "loading" && (
        <div id={popupId} role="status">Loading users…</div>
      )}
      {open && phase === "error" && (
        <div id={popupId} role="alert">Unable to load users.</div>
      )}
      {open && phase === "results" && results.length === 0 && (
        <div id={popupId} role="status">No users found.</div>
      )}
      {open && phase === "results" && results.length > 0 && (
        <ul id={popupId} role="listbox">
          {results.map((user, index) => (
            <li
              id={`${baseId}-option-${index}`}
              key={user.id}
              role="option"
              aria-selected={index === activeIndex}
              onMouseDown={(event) => event.preventDefault()}
              onClick={() => selectUser(user)}
            >
              {user.name}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
