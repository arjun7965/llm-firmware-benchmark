# Reference

`pathutil.py` is the trusted implementation. `test_normalize_path.py` is the
trusted model-answer fixture: it contains an independent character-scanning
oracle, shrinking-friendly structured generators, targeted boundaries, and
metamorphic properties. Calibration holds the test module fixed while applying
the controlled defects in `mutations.json` to the supplied implementation.
