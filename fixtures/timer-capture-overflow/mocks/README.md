# Timer Capture/Compare Mock

The mock keeps TIMER1 opaque, advances its 16-bit counter deterministically,
latches capture/overflow/compare status, and records each accessor call. Tests
control delayed status delivery without host threads.
