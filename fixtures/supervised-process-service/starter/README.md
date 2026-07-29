# Starter Contract

`supervised_service.h` declares the candidate API and fixed resource limits.
`supervisor_os.h` declares the supplied process-launch boundary. The launcher
creates one worker process, one pidfd, and one nonblocking `SOCK_SEQPACKET`
channel whose child endpoint is descriptor 3.
