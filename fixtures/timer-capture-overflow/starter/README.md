# Timer Capture/Compare Starter Contract

Implement `timer_capture.h` using only the opaque TIMER1 and interrupt-mask
accessors declared by `fixture_timer_capture.h`. The caller owns
`timer_capture_t`; the fixture uses one retained capture sample and an explicit
software high word to reconstruct 32-bit timestamps.
