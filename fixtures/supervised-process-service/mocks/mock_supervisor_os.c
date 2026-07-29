#include "mock_supervisor_os.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  char path[256];
} spawn_call_t;

typedef struct {
  pid_t pid;
  int signal_number;
} kill_call_t;

typedef struct {
  pid_t pid;
  int options;
} wait_call_t;

typedef struct {
  int signal_number;
  bool is_restore;
  int flags;
} signal_call_t;

static mock_supervisor_spawn_step_t spawn_steps[MOCK_SUPERVISOR_MAX_CALLS];
static size_t spawn_step_count;
static size_t spawn_step_index;
static spawn_call_t spawn_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t spawn_call_count;

static mock_supervisor_poll_step_t poll_steps[MOCK_SUPERVISOR_MAX_CALLS];
static size_t poll_step_count;
static size_t poll_step_index;
static mock_supervisor_poll_call_t poll_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t poll_call_count;

static mock_supervisor_result_step_t send_steps[MOCK_SUPERVISOR_MAX_CALLS];
static size_t send_step_count;
static size_t send_step_index;
static mock_supervisor_io_call_t send_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t send_call_count;

static mock_supervisor_io_step_t recv_steps[MOCK_SUPERVISOR_MAX_CALLS];
static size_t recv_step_count;
static size_t recv_step_index;
static mock_supervisor_io_call_t recv_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t recv_call_count;

static mock_supervisor_result_step_t kill_steps[MOCK_SUPERVISOR_MAX_CALLS];
static size_t kill_step_count;
static size_t kill_step_index;
static kill_call_t kill_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t kill_call_count;

static mock_supervisor_wait_step_t wait_steps[MOCK_SUPERVISOR_MAX_CALLS];
static size_t wait_step_count;
static size_t wait_step_index;
static wait_call_t wait_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t wait_call_count;

static int close_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t close_call_count;
static struct sigaction current_actions[NSIG];
static signal_call_t signal_calls[MOCK_SUPERVISOR_MAX_CALLS];
static size_t signal_call_count;
static size_t pipe2_call_count;
static int pipe2_flags;
static int pipe2_error;
static size_t failing_sigaction_call;
static int sigaction_error;
static int wake_write_error;
static size_t wake_write_count;
static int observed_signal_handler_errno;

static void require_capacity(size_t count, const char *name) {
  if (count >= MOCK_SUPERVISOR_MAX_CALLS) {
    fprintf(stderr, "mock %s capacity exceeded\n", name);
    abort();
  }
}

static void copy_bytes(
  uint8_t *destination,
  size_t destination_size,
  const void *source,
  size_t source_size
) {
  const size_t copied = source_size < destination_size
    ? source_size
    : destination_size;

  if (copied > 0u) memcpy(destination, source, copied);
}

void mock_supervisor_reset(void) {
  size_t index;

  memset(spawn_steps, 0, sizeof(spawn_steps));
  memset(spawn_calls, 0, sizeof(spawn_calls));
  memset(poll_steps, 0, sizeof(poll_steps));
  memset(poll_calls, 0, sizeof(poll_calls));
  memset(send_steps, 0, sizeof(send_steps));
  memset(send_calls, 0, sizeof(send_calls));
  memset(recv_steps, 0, sizeof(recv_steps));
  memset(recv_calls, 0, sizeof(recv_calls));
  memset(kill_steps, 0, sizeof(kill_steps));
  memset(kill_calls, 0, sizeof(kill_calls));
  memset(wait_steps, 0, sizeof(wait_steps));
  memset(wait_calls, 0, sizeof(wait_calls));
  memset(close_calls, 0, sizeof(close_calls));
  memset(signal_calls, 0, sizeof(signal_calls));
  spawn_step_count = 0u;
  spawn_step_index = 0u;
  spawn_call_count = 0u;
  poll_step_count = 0u;
  poll_step_index = 0u;
  poll_call_count = 0u;
  send_step_count = 0u;
  send_step_index = 0u;
  send_call_count = 0u;
  recv_step_count = 0u;
  recv_step_index = 0u;
  recv_call_count = 0u;
  kill_step_count = 0u;
  kill_step_index = 0u;
  kill_call_count = 0u;
  wait_step_count = 0u;
  wait_step_index = 0u;
  wait_call_count = 0u;
  close_call_count = 0u;
  signal_call_count = 0u;
  pipe2_call_count = 0u;
  pipe2_flags = 0;
  pipe2_error = 0;
  failing_sigaction_call = 0u;
  sigaction_error = 0;
  wake_write_error = 0;
  wake_write_count = 0u;
  observed_signal_handler_errno = -1;
  for (index = 0u; index < NSIG; ++index) {
    memset(&current_actions[index], 0, sizeof(current_actions[index]));
    current_actions[index].sa_handler = SIG_DFL;
    (void)sigemptyset(&current_actions[index].sa_mask);
  }
  errno = 0;
}

void mock_supervisor_queue_spawn(mock_supervisor_spawn_step_t step) {
  require_capacity(spawn_step_count, "spawn step");
  spawn_steps[spawn_step_count++] = step;
}

void mock_supervisor_queue_poll(mock_supervisor_poll_step_t step) {
  require_capacity(poll_step_count, "poll step");
  poll_steps[poll_step_count++] = step;
}

void mock_supervisor_queue_send(ssize_t result, int error_number) {
  require_capacity(send_step_count, "send step");
  send_steps[send_step_count++] = (mock_supervisor_result_step_t){
    .result = (int)result,
    .error_number = error_number,
  };
}

void mock_supervisor_queue_recv(
  const uint8_t *data,
  size_t data_length,
  ssize_t result,
  int error_number
) {
  mock_supervisor_io_step_t *step;

  require_capacity(recv_step_count, "recv step");
  if (data_length > MOCK_SUPERVISOR_MAX_FRAME_BYTES) {
    fprintf(stderr, "mock recv data too large\n");
    abort();
  }
  step = &recv_steps[recv_step_count++];
  step->result = result;
  step->error_number = error_number;
  step->data_length = data_length;
  if (data_length > 0u) {
    if (data == NULL) abort();
    memcpy(step->data, data, data_length);
  }
}

void mock_supervisor_queue_kill(int result, int error_number) {
  require_capacity(kill_step_count, "kill step");
  kill_steps[kill_step_count++] = (mock_supervisor_result_step_t){
    .result = result,
    .error_number = error_number,
  };
}

void mock_supervisor_queue_waitpid(
  pid_t result,
  int error_number,
  int status
) {
  require_capacity(wait_step_count, "waitpid step");
  wait_steps[wait_step_count++] = (mock_supervisor_wait_step_t){
    .result = result,
    .error_number = error_number,
    .status = status,
  };
}

void mock_supervisor_fail_pipe2(int error_number) {
  pipe2_error = error_number;
}

void mock_supervisor_fail_sigaction_call(
  size_t call_number,
  int error_number
) {
  failing_sigaction_call = call_number;
  sigaction_error = error_number;
}

void mock_supervisor_fail_wake_write(int error_number) {
  wake_write_error = error_number;
}

int supervisor_os_spawn_worker(
  const char *worker_path,
  pid_t *pid_out,
  int *pidfd_out,
  int *channel_fd_out
) {
  mock_supervisor_spawn_step_t step;

  require_capacity(spawn_call_count, "spawn call");
  if (
    worker_path == NULL ||
    pid_out == NULL ||
    pidfd_out == NULL ||
    channel_fd_out == NULL
  ) {
    abort();
  }
  (void)snprintf(
    spawn_calls[spawn_call_count++].path,
    sizeof(spawn_calls[0].path),
    "%s",
    worker_path
  );
  if (spawn_step_index >= spawn_step_count) {
    errno = EACCES;
    return -1;
  }
  step = spawn_steps[spawn_step_index++];
  if (step.result != 0) {
    errno = step.error_number;
    return -1;
  }
  *pid_out = step.pid;
  *pidfd_out = step.pidfd;
  *channel_fd_out = step.channel_fd;
  return 0;
}

int fixture_supervisor_close(int fd) {
  require_capacity(close_call_count, "close call");
  close_calls[close_call_count++] = fd;
  return 0;
}

ssize_t fixture_supervisor_write(
  int fd,
  const void *buffer,
  size_t count
) {
  (void)buffer;
  if (fd != MOCK_SUPERVISOR_WAKE_WRITE_FD || count != 1u) abort();
  ++wake_write_count;
  if (wake_write_error != 0) {
    errno = wake_write_error;
    return -1;
  }
  return 1;
}

int fixture_supervisor_poll(
  struct pollfd *fds,
  nfds_t descriptor_count,
  int timeout_ms
) {
  mock_supervisor_poll_step_t step;
  mock_supervisor_poll_call_t *call;
  nfds_t index;

  require_capacity(poll_call_count, "poll call");
  if (descriptor_count == 0u || descriptor_count > 3u || fds == NULL) abort();
  call = &poll_calls[poll_call_count++];
  call->descriptor_count = descriptor_count;
  call->timeout_ms = timeout_ms;
  for (index = 0u; index < descriptor_count; ++index) {
    call->fds[index] = fds[index].fd;
    call->events[index] = fds[index].events;
  }
  if (poll_step_index >= poll_step_count) {
    errno = EINVAL;
    return -1;
  }
  step = poll_steps[poll_step_index++];
  for (index = 0u; index < descriptor_count; ++index) {
    fds[index].revents = step.revents[index];
  }
  if (step.signal_number != 0) {
    const struct sigaction *action = &current_actions[step.signal_number];

    if (action->sa_handler == SIG_DFL || action->sa_handler == SIG_IGN) abort();
    errno = 777;
    action->sa_handler(step.signal_number);
    observed_signal_handler_errno = errno;
  }
  if (step.result < 0) errno = step.error_number;
  return step.result;
}

int fixture_supervisor_pipe2(int pipe_fds[2], int flags) {
  ++pipe2_call_count;
  pipe2_flags = flags;
  if (pipe2_error != 0) {
    errno = pipe2_error;
    return -1;
  }
  pipe_fds[0] = MOCK_SUPERVISOR_WAKE_READ_FD;
  pipe_fds[1] = MOCK_SUPERVISOR_WAKE_WRITE_FD;
  return 0;
}

int fixture_supervisor_sigaction(
  int signal_number,
  const struct sigaction *action,
  struct sigaction *old_action
) {
  signal_call_t *call;

  require_capacity(signal_call_count, "sigaction call");
  call = &signal_calls[signal_call_count++];
  call->signal_number = signal_number;
  call->is_restore = old_action == NULL;
  call->flags = action == NULL ? -1 : action->sa_flags;
  if (
    failing_sigaction_call != 0u &&
    signal_call_count == failing_sigaction_call
  ) {
    errno = sigaction_error;
    return -1;
  }
  if (old_action != NULL) *old_action = current_actions[signal_number];
  if (action != NULL) current_actions[signal_number] = *action;
  return 0;
}

int fixture_supervisor_sigemptyset(sigset_t *set) {
  return sigemptyset(set);
}

ssize_t fixture_supervisor_send(
  int fd,
  const void *buffer,
  size_t length,
  int flags
) {
  mock_supervisor_io_call_t *call;
  mock_supervisor_result_step_t step;

  require_capacity(send_call_count, "send call");
  call = &send_calls[send_call_count++];
  call->fd = fd;
  call->length = length;
  call->flags = flags;
  copy_bytes(call->data, sizeof(call->data), buffer, length);
  if (send_step_index >= send_step_count) return (ssize_t)length;
  step = send_steps[send_step_index++];
  if (step.result < 0) errno = step.error_number;
  return (ssize_t)step.result;
}

ssize_t fixture_supervisor_recv(
  int fd,
  void *buffer,
  size_t length,
  int flags
) {
  mock_supervisor_io_call_t *call;
  mock_supervisor_io_step_t step;

  require_capacity(recv_call_count, "recv call");
  call = &recv_calls[recv_call_count++];
  call->fd = fd;
  call->length = length;
  call->flags = flags;
  if (recv_step_index >= recv_step_count) {
    errno = EINVAL;
    return -1;
  }
  step = recv_steps[recv_step_index++];
  if (step.result < 0) {
    errno = step.error_number;
    return step.result;
  }
  copy_bytes(buffer, length, step.data, step.data_length);
  return step.result;
}

int fixture_supervisor_kill(pid_t pid, int signal_number) {
  mock_supervisor_result_step_t step = {0};

  require_capacity(kill_call_count, "kill call");
  kill_calls[kill_call_count++] = (kill_call_t){
    .pid = pid,
    .signal_number = signal_number,
  };
  if (kill_step_index < kill_step_count) {
    step = kill_steps[kill_step_index++];
  }
  if (step.result < 0) errno = step.error_number;
  return step.result;
}

pid_t fixture_supervisor_waitpid(pid_t pid, int *status, int options) {
  mock_supervisor_wait_step_t step = {
    .result = pid,
    .error_number = 0,
    .status = 0,
  };

  require_capacity(wait_call_count, "waitpid call");
  wait_calls[wait_call_count++] = (wait_call_t){
    .pid = pid,
    .options = options,
  };
  if (wait_step_index < wait_step_count) {
    step = wait_steps[wait_step_index++];
  }
  if (step.result < 0) {
    errno = step.error_number;
    return step.result;
  }
  if (status != NULL) *status = step.status;
  return step.result;
}

size_t mock_supervisor_spawn_call_count(void) {
  return spawn_call_count;
}

const char *mock_supervisor_spawn_path(size_t index) {
  if (index >= spawn_call_count) abort();
  return spawn_calls[index].path;
}

size_t mock_supervisor_poll_call_count(void) {
  return poll_call_count;
}

mock_supervisor_poll_call_t mock_supervisor_poll_call(size_t index) {
  if (index >= poll_call_count) abort();
  return poll_calls[index];
}

size_t mock_supervisor_send_call_count(void) {
  return send_call_count;
}

mock_supervisor_io_call_t mock_supervisor_send_call(size_t index) {
  if (index >= send_call_count) abort();
  return send_calls[index];
}

size_t mock_supervisor_recv_call_count(void) {
  return recv_call_count;
}

mock_supervisor_io_call_t mock_supervisor_recv_call(size_t index) {
  if (index >= recv_call_count) abort();
  return recv_calls[index];
}

size_t mock_supervisor_kill_call_count(void) {
  return kill_call_count;
}

pid_t mock_supervisor_kill_pid(size_t index) {
  if (index >= kill_call_count) abort();
  return kill_calls[index].pid;
}

int mock_supervisor_kill_signal(size_t index) {
  if (index >= kill_call_count) abort();
  return kill_calls[index].signal_number;
}

size_t mock_supervisor_wait_call_count(void) {
  return wait_call_count;
}

pid_t mock_supervisor_wait_pid(size_t index) {
  if (index >= wait_call_count) abort();
  return wait_calls[index].pid;
}

int mock_supervisor_wait_options(size_t index) {
  if (index >= wait_call_count) abort();
  return wait_calls[index].options;
}

size_t mock_supervisor_close_call_count(void) {
  return close_call_count;
}

int mock_supervisor_close_fd(size_t index) {
  if (index >= close_call_count) abort();
  return close_calls[index];
}

size_t mock_supervisor_sigaction_call_count(void) {
  return signal_call_count;
}

int mock_supervisor_sigaction_signal(size_t index) {
  if (index >= signal_call_count) abort();
  return signal_calls[index].signal_number;
}

bool mock_supervisor_sigaction_is_restore(size_t index) {
  if (index >= signal_call_count) abort();
  return signal_calls[index].is_restore;
}

int mock_supervisor_sigaction_flags(size_t index) {
  if (index >= signal_call_count) abort();
  return signal_calls[index].flags;
}

size_t mock_supervisor_wake_write_count(void) {
  return wake_write_count;
}

int mock_supervisor_signal_handler_errno(void) {
  return observed_signal_handler_errno;
}

size_t mock_supervisor_pipe2_call_count(void) {
  return pipe2_call_count;
}

int mock_supervisor_pipe2_flags(void) {
  return pipe2_flags;
}
