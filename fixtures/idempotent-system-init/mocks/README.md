# Idempotent System-Init Mocks

The synchronous deterministic mock tracks controller writes, values, and IRQ
save/restore operations. Optional watched caller state and retained-record
snapshots at accessor boundaries verify updates before interrupt restoration.
The watchers are test-only; generated implementations use only the supplied
opaque accessors and cannot depend on the observer API.

Calls are serialized, and reset/corruption occur between completed calls. The
mock does not model torn stores, power loss within a call, physical durability,
actual preemption, or adversarial checksum collisions.
