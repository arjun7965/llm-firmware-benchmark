# Supervisor OS Mock

`redirect_posix.h` redirects the candidate's signal, polling, IPC, child
termination, reaping, and descriptor operations. `mock_supervisor_os.c` also
implements the supplied worker-launch boundary. Tests can script process exits,
malformed acknowledgements, timeouts, signals, and restart/shutdown behavior
without creating a real child process.

Poll scripts name semantic roles (wake-pipe read end, current worker pidfd,
current worker channel), independently of the candidate's poll array order.
Blocking polls advance the scenario. Zero-time probes observe recorded pidfd
readiness without consuming a future event; spawning the next worker clears
that state. Returned readiness is masked by requested interests, except for
unconditional terminal flags. Unknown descriptors receive `POLLNVAL`.
This is a deterministic lifecycle script, not a general POSIX simulator.
