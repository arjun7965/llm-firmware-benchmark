#ifndef SUPERVISOR_OS_H
#define SUPERVISOR_OS_H

#include <sys/types.h>

#define SUPERVISOR_WORKER_CHANNEL_FD 3

/*
 * Starts worker_path with no inherited supervisor descriptors except a
 * nonblocking SOCK_SEQPACKET channel at SUPERVISOR_WORKER_CHANNEL_FD.
 *
 * On success, returns 0 and stores the child PID, a close-on-exec pidfd, and
 * the supervisor's nonblocking close-on-exec channel endpoint. On failure,
 * returns -1, sets errno, and leaves no child or open descriptor behind.
 */
int supervisor_os_spawn_worker(
  const char *worker_path,
  pid_t *pid_out,
  int *pidfd_out,
  int *channel_fd_out
);

#endif
