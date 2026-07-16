# GPIO Edge/Debounce Starter Contract

Implement `gpio_debounce.h` as freestanding C11. The button is active low:
the `GPIO0_BUTTON_MASK` input bit is clear when pressed. GPIO0 is opaque; use
only the accessors declared in `fixture_gpio_debounce.h`.

The driver runs on a single-core Cortex-M3. `gpio_debounce_irq` is a
non-nested GPIO ISR. All other functions run in foreground code and must use
the supplied interrupt save/restore pair whenever they inspect or modify
state shared with the ISR.
