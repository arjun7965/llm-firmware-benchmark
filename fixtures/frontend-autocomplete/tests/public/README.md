# Public tests

The TypeScript/React interaction suite runs against jsdom with a validator-owned
fake timer. It checks the exact 250 ms boundary, debounce replacement,
AbortController cleanup, stale-response fencing, accessible feedback,
combobox/listbox relationships, wrapped keyboard navigation, Escape, Enter,
mouse selection, empty-query reset, and unmount cancellation.
