#ifndef MOCK_SUPERVISOR_OS_H
#define MOCK_SUPERVISOR_OS_H

#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define MOCK_SUPERVISOR_MAX_CALLS 256u
#define MOCK_SUPERVISOR_MAX_FRAME_BYTES 64u
#define MOCK_SUPERVISOR_WAKE_READ_FD 70
#define MOCK_SUPERVISOR_WAKE_WRITE_FD 71

typedef struct {
  int result;
  int error_number;
  pid_t pid;
  int pidfd;
  int channel_fd;
} mock_supervisor_spawn_step_t;

typedef struct {
  int result;
  int error_number;
  short revents[3];
  int signal_number;
} mock_supervisor_poll_step_t;

typedef struct {
  ssize_t result;
  int error_number;
  uint8_t data[MOCK_SUPERVISOR_MAX_FRAME_BYTES];
  size_t data_length;
} mock_supervisor_io_step_t;

typedef struct {
  pid_t result;
  int error_number;
  int status;
} mock_supervisor_wait_step_t;

typedef struct {
  int result;
  int error_number;
} mock_supervisor_result_step_t;

typedef struct {
  nfds_t descriptor_count;
  int timeout_ms;
  int fds[3];
  short events[3];
} mock_supervisor_poll_call_t;

typedef struct {
  int fd;
  size_t length;
  int flags;
  uint8_t data[MOCK_SUPERVISOR_MAX_FRAME_BYTES];
} mock_supervisor_io_call_t;

int supervisor_os_spawn_worker(
  const char *worker_path,
  pid_t *pid_out,
  int *pidfd_out,
  int *channel_fd_out
);
int fixture_supervisor_close(int fd);
ssize_t fixture_supervisor_write(
  int fd,
  const void *buffer,
  size_t count
);
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

void mock_supervisor_reset(void);
void mock_supervisor_queue_spawn(mock_supervisor_spawn_step_t step);
void mock_supervisor_queue_poll(mock_supervisor_poll_step_t step);
void mock_supervisor_queue_send(ssize_t result, int error_number);
void mock_supervisor_queue_recv(
  const uint8_t *data,
  size_t data_length,
  ssize_t result,
  int error_number
);
void mock_supervisor_queue_kill(int result, int error_number);
void mock_supervisor_queue_waitpid(
  pid_t result,
  int error_number,
  int status
);
void mock_supervisor_fail_pipe2(int error_number);
void mock_supervisor_fail_sigaction_call(
  size_t call_number,
  int error_number
);
void mock_supervisor_fail_wake_write(int error_number);

size_t mock_supervisor_spawn_call_count(void);
const char *mock_supervisor_spawn_path(size_t index);
size_t mock_supervisor_poll_call_count(void);
mock_supervisor_poll_call_t mock_supervisor_poll_call(size_t index);
size_t mock_supervisor_send_call_count(void);
mock_supervisor_io_call_t mock_supervisor_send_call(size_t index);
size_t mock_supervisor_recv_call_count(void);
mock_supervisor_io_call_t mock_supervisor_recv_call(size_t index);
size_t mock_supervisor_kill_call_count(void);
pid_t mock_supervisor_kill_pid(size_t index);
int mock_supervisor_kill_signal(size_t index);
size_t mock_supervisor_wait_call_count(void);
pid_t mock_supervisor_wait_pid(size_t index);
int mock_supervisor_wait_options(size_t index);
size_t mock_supervisor_close_call_count(void);
int mock_supervisor_close_fd(size_t index);
size_t mock_supervisor_sigaction_call_count(void);
int mock_supervisor_sigaction_signal(size_t index);
bool mock_supervisor_sigaction_is_restore(size_t index);
int mock_supervisor_sigaction_flags(size_t index);
size_t mock_supervisor_wake_write_count(void);
int mock_supervisor_signal_handler_errno(void);
size_t mock_supervisor_pipe2_call_count(void);
int mock_supervisor_pipe2_flags(void);

#endif
