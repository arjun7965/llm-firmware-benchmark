# Supervisor OS Mock

`redirect_posix.h` redirects the candidate's signal, polling, IPC, child
termination, reaping, and descriptor operations. `mock_supervisor_os.c` also
implements the supplied worker-launch boundary. Tests can script process exits,
malformed acknowledgements, timeouts, signals, and restart/shutdown behavior
without creating a real child process.
