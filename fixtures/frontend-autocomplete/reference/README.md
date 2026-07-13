# Trusted reference

`Autocomplete.tsx` implements the exact default-exported prop contract. It
uses both request abortion and a monotonically increasing generation fence,
so a search implementation that ignores its signal still cannot publish stale
state.
