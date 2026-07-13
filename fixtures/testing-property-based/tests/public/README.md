# Public Validator Configuration

The model answer is itself the pytest test module, so the validator-owned public
asset is the fixed pytest configuration. It exposes the supplied `starter/`
implementation on the import path and runs the answer with a deterministic
Hypothesis seed, no network, and no pytest cache writes.
