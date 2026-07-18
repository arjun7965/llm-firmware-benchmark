# Trusted Reference

The reference creates one event group and two mutexes, waits for any declared
event with clear-on-exit semantics, and always attempts configuration before
actuator locking. Each lock has a one-tick bound; failed second acquisition
releases the first lock before returning.
