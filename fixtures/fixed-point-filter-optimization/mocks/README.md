# Cost Model Mock

The opaque mock implements an architecture-independent cycle contract. It
records declared and observed MAC counts, sample/coefficient operands, consumed
cycles, and commit state. Tests can limit available cycles or force a commit
failure without exposing the model internals to candidate code.
