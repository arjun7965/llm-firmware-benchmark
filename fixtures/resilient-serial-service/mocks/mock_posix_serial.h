#ifndef MOCK_POSIX_SERIAL_H
#define MOCK_POSIX_SERIAL_H

#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <termios.h>

#define MOCK_POSIX_MAX_CALLS 128u
#define MOCK_POSIX_WAKE_READ_FD 70
#define MOCK_POSIX_WAKE_WRITE_FD 71

typedef struct {
  int result;
  int error_number;
} mock_posix_result_t;

typedef struct {
  int result;
  int error_number;
  short serial_revents;
  short wake_revents;
  int signal_number;
} mock_posix_poll_step_t;

typedef struct {
  int result;
  int error_number;
  uint8_t data[128];
  size_t data_length;
} mock_posix_read_step_t;

typedef struct {
  nfds_t descriptor_count;
  int timeout_ms;
  int wake_fd;
  short wake_events;
  int serial_fd;
  short serial_events;
} mock_posix_poll_call_t;

typedef struct {
  int fd;
  size_t count;
} mock_posix_io_call_t;

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

void mock_posix_reset(void);
void mock_posix_queue_open(int result, int error_number);
void mock_posix_queue_poll(mock_posix_poll_step_t step);
void mock_posix_queue_read_data(const uint8_t *data, size_t length);
void mock_posix_queue_read_result(int result, int error_number);
void mock_posix_fail_pipe2(int error_number);
void mock_posix_fail_sigaction_call(size_t call_number, int error_number);
void mock_posix_fail_tcgetattr_call(size_t call_number, int error_number);
void mock_posix_fail_tcsetattr_call(size_t call_number, int error_number);
void mock_posix_fail_wake_write(int error_number);

size_t mock_posix_open_call_count(void);
const char *mock_posix_open_path(size_t index);
int mock_posix_open_flags(size_t index);
size_t mock_posix_poll_call_count(void);
mock_posix_poll_call_t mock_posix_poll_call(size_t index);
size_t mock_posix_read_call_count(void);
mock_posix_io_call_t mock_posix_read_call(size_t index);
size_t mock_posix_wake_write_count(void);
size_t mock_posix_close_call_count(void);
int mock_posix_close_fd(size_t index);
size_t mock_posix_sigaction_call_count(void);
int mock_posix_sigaction_signal(size_t index);
bool mock_posix_sigaction_is_restore(size_t index);
int mock_posix_sigaction_flags(size_t index);
int mock_posix_signal_handler_errno(void);
size_t mock_posix_tcgetattr_call_count(void);
size_t mock_posix_tcsetattr_call_count(void);
int mock_posix_tcsetattr_fd(void);
int mock_posix_tcsetattr_action(void);
const struct termios *mock_posix_tcsetattr_attributes(void);
size_t mock_posix_pipe2_call_count(void);
int mock_posix_pipe2_flags(void);

#endif
