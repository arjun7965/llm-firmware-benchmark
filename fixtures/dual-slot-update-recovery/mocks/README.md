# FLASH0 Mock

The deterministic FLASH0 mock records erase, bounded word programming, verifier
outcomes, boot-target selection, and exact foreground interrupt restoration.

An optional watched journal is copied into each accessor event so tests can
inspect retained state at erase, program, verification, boot selection, and
interrupt-restoration boundaries. This models completed journal transitions;
it does not simulate byte-level torn writes or physical flash durability.
