#ifndef REDIRECT_SUPERVISOR_POSIX_H
#define REDIRECT_SUPERVISOR_POSIX_H

#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int fixture_supervisor_close(int fd);
ssize_t fixture_supervisor_write(int fd, const void *buffer, size_t count);
int fixture_supervisor_poll(
  struct pollfd *fds,
  nfds_t descriptor_count,
  int timeout_ms
);
int fixture_supervisor_pipe2(int pipe_fds[2], int flags);
int fixture_supervisor_sigaction(
  int signal_number,
  const struct sigaction *action,
  struct sigaction *old_action
);
int fixture_supervisor_sigemptyset(sigset_t *set);
ssize_t fixture_supervisor_send(
  int fd,
  const void *buffer,
  size_t length,
  int flags
);
ssize_t fixture_supervisor_recv(
  int fd,
  void *buffer,
  size_t length,
  int flags
);
int fixture_supervisor_kill(pid_t pid, int signal_number);
pid_t fixture_supervisor_waitpid(pid_t pid, int *status, int options);

#define close fixture_supervisor_close
#define write fixture_supervisor_write
#define poll fixture_supervisor_poll
#define pipe2 fixture_supervisor_pipe2
#define sigaction(...) fixture_supervisor_sigaction(__VA_ARGS__)
#define sigemptyset fixture_supervisor_sigemptyset
#define send fixture_supervisor_send
#define recv fixture_supervisor_recv
#define kill fixture_supervisor_kill
#define waitpid fixture_supervisor_waitpid

#endif
