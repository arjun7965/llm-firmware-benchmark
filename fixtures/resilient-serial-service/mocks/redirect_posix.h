#ifndef REDIRECT_POSIX_H
#define REDIRECT_POSIX_H

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

int fixture_posix_open(const char *path, int flags, ...);
int fixture_posix_close(int fd);
ssize_t fixture_posix_read(int fd, void *buffer, size_t count);
ssize_t fixture_posix_write(int fd, const void *buffer, size_t count);
int fixture_posix_poll(struct pollfd *fds, nfds_t count, int timeout_ms);
int fixture_posix_pipe2(int pipe_fds[2], int flags);
int fixture_posix_sigaction(
  int signal_number,
  const struct sigaction *action,
  struct sigaction *old_action
);
int fixture_posix_sigemptyset(sigset_t *set);
int fixture_posix_tcgetattr(int fd, struct termios *attributes);
int fixture_posix_tcsetattr(
  int fd,
  int optional_actions,
  const struct termios *attributes
);

#define open fixture_posix_open
#define close fixture_posix_close
#define read fixture_posix_read
#define write fixture_posix_write
#define poll fixture_posix_poll
#define pipe2 fixture_posix_pipe2
#define sigaction(...) fixture_posix_sigaction(__VA_ARGS__)
#define sigemptyset fixture_posix_sigemptyset
#define tcgetattr fixture_posix_tcgetattr
#define tcsetattr fixture_posix_tcsetattr

#endif
