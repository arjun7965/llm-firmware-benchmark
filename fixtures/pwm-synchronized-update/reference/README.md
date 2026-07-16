# Trusted PWM Synchronized-Update Reference

This validator-owned implementation uses PWM shadow registers for boundary
aligned duty changes, gives a hardware fault priority over a simultaneous
update, and requires explicit recovery before output resumes.
