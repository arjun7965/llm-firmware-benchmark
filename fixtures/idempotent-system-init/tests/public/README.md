# Idempotent System-Init Public Tests

Fourteen groups cover strict programming order; inclusive clock/mask boundaries;
all retained and state fields on repeat/conflict rejection; isolated retained
corruption and structural invalidity; saturated counts across reboots; retained
safe boot with changed requested config; transition gates and empty events;
non-usable state; exact opaque IRQ restoration including zero; publication
before restore; config aliasing; structurally valid mismatched resume records;
and complete invalid-record repair before initial controller programming. The last group was added after generation
and is disclosed separately from the original thirteen-group results.

The trusted reference and controlled catalog must pass sandbox calibration before
generation. Tests compare named struct fields rather than padding bytes.
