#include "mock_posix_serial.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  char path[256];
  int flags;
} open_call_t;

typedef struct {
  int signal_number;
  bool is_restore;
  int flags;
} signal_call_t;

static mock_posix_result_t open_steps[MOCK_POSIX_MAX_CALLS];
static size_t open_step_count;
static size_t open_step_index;
static open_call_t open_calls[MOCK_POSIX_MAX_CALLS];
static size_t open_calls_count;

static mock_posix_poll_step_t poll_steps[MOCK_POSIX_MAX_CALLS];
static size_t poll_step_count;
static size_t poll_step_index;
static mock_posix_poll_call_t poll_calls[MOCK_POSIX_MAX_CALLS];
static size_t poll_calls_count;

static mock_posix_read_step_t read_steps[MOCK_POSIX_MAX_CALLS];
static size_t read_step_count;
static size_t read_step_index;
static mock_posix_io_call_t read_calls[MOCK_POSIX_MAX_CALLS];
static size_t read_calls_count;

static int close_calls[MOCK_POSIX_MAX_CALLS];
static size_t close_calls_count;
static size_t wake_write_calls;

static struct sigaction current_actions[NSIG];
static signal_call_t signal_calls[MOCK_POSIX_MAX_CALLS];
static size_t signal_calls_count;
static int observed_signal_handler_errno;

static struct termios configured_attributes;
static int configured_fd;
static int configured_action;
static size_t tcgetattr_calls;
static size_t tcsetattr_calls;

static size_t pipe2_calls;
static int recorded_pipe2_flags;
static int pipe2_error;
static size_t failing_sigaction_call;
static int sigaction_error;
static size_t failing_tcgetattr_call;
static int tcgetattr_error;
static size_t failing_tcsetattr_call;
static int tcsetattr_error;
static int wake_write_error;

static void require_capacity(size_t count, const char *name) {
  if (count >= MOCK_POSIX_MAX_CALLS) {
    fprintf(stderr, "mock %s capacity exceeded\n", name);
    abort();
  }
}

static void initialize_termios(struct termios *attributes) {
  memset(attributes, 0, sizeof(*attributes));
  attributes->c_iflag =
    IGNBRK |
    BRKINT |
    PARMRK |
    ISTRIP |
    INLCR |
    IGNCR |
    ICRNL |
    IXON |
    IXOFF |
    IXANY;
  attributes->c_oflag = OPOST;
  attributes->c_lflag = ECHO | ECHONL | ICANON | ISIG | IEXTEN;
  attributes->c_cflag =
    CS7 |
    PARENB |
    PARODD |
    CSTOPB |
    CRTSCTS;
  attributes->c_cc[VMIN] = 9;
  attributes->c_cc[VTIME] = 7;
  (void)cfsetispeed(attributes, B9600);
  (void)cfsetospeed(attributes, B9600);
}

void mock_posix_reset(void) {
  size_t index;

  memset(open_steps, 0, sizeof(open_steps));
  memset(open_calls, 0, sizeof(open_calls));
  memset(poll_steps, 0, sizeof(poll_steps));
  memset(poll_calls, 0, sizeof(poll_calls));
  memset(read_steps, 0, sizeof(read_steps));
  memset(read_calls, 0, sizeof(read_calls));
  memset(close_calls, 0, sizeof(close_calls));
  memset(signal_calls, 0, sizeof(signal_calls));
  memset(&configured_attributes, 0, sizeof(configured_attributes));
  open_step_count = 0;
  open_step_index = 0;
  open_calls_count = 0;
  poll_step_count = 0;
  poll_step_index = 0;
  poll_calls_count = 0;
  read_step_count = 0;
  read_step_index = 0;
  read_calls_count = 0;
  close_calls_count = 0;
  wake_write_calls = 0;
  signal_calls_count = 0;
  observed_signal_handler_errno = -1;
  tcgetattr_calls = 0;
  tcsetattr_calls = 0;
  configured_fd = -1;
  configured_action = -1;
  pipe2_calls = 0;
  recorded_pipe2_flags = 0;
  pipe2_error = 0;
  failing_sigaction_call = 0;
  sigaction_error = 0;
  failing_tcgetattr_call = 0;
  tcgetattr_error = 0;
  failing_tcsetattr_call = 0;
  tcsetattr_error = 0;
  wake_write_error = 0;
  for (index = 0; index < NSIG; ++index) {
    memset(&current_actions[index], 0, sizeof(current_actions[index]));
    current_actions[index].sa_handler = SIG_DFL;
    (void)sigemptyset(&current_actions[index].sa_mask);
  }
  errno = 0;
}

void mock_posix_queue_open(int result, int error_number) {
  require_capacity(open_step_count, "open step");
  open_steps[open_step_count++] = (mock_posix_result_t){
    .result = result,
    .error_number = error_number,
  };
}

void mock_posix_queue_poll(mock_posix_poll_step_t step) {
  require_capacity(poll_step_count, "poll step");
  poll_steps[poll_step_count++] = step;
}

void mock_posix_queue_read_data(const uint8_t *data, size_t length) {
  mock_posix_read_step_t *step;

  require_capacity(read_step_count, "read step");
  if (data == NULL || length > sizeof(read_steps[0].data)) {
    fprintf(stderr, "invalid mock read data\n");
    abort();
  }
  step = &read_steps[read_step_count++];
  step->result = (int)length;
  step->data_length = length;
  memcpy(step->data, data, length);
}

void mock_posix_queue_read_result(int result, int error_number) {
  require_capacity(read_step_count, "read step");
  read_steps[read_step_count++] = (mock_posix_read_step_t){
    .result = result,
    .error_number = error_number,
  };
}

void mock_posix_fail_pipe2(int error_number) {
  pipe2_error = error_number;
}

void mock_posix_fail_sigaction_call(
  size_t call_number,
  int error_number
) {
  failing_sigaction_call = call_number;
  sigaction_error = error_number;
}

void mock_posix_fail_tcgetattr_call(
  size_t call_number,
  int error_number
) {
  failing_tcgetattr_call = call_number;
  tcgetattr_error = error_number;
}

void mock_posix_fail_tcsetattr_call(
  size_t call_number,
  int error_number
) {
  failing_tcsetattr_call = call_number;
  tcsetattr_error = error_number;
}

void mock_posix_fail_wake_write(int error_number) {
  wake_write_error = error_number;
}

int fixture_posix_open(const char *path, int flags, ...) {
  mock_posix_result_t step;
  open_call_t *call;

  require_capacity(open_calls_count, "open call");
  call = &open_calls[open_calls_count++];
  call->flags = flags;
  if (path != NULL) {
    (void)snprintf(call->path, sizeof(call->path), "%s", path);
  }
  if (open_step_index >= open_step_count) {
    errno = EACCES;
    return -1;
  }
  step = open_steps[open_step_index++];
  if (step.result < 0) errno = step.error_number;
  return step.result;
}

int fixture_posix_close(int fd) {
  require_capacity(close_calls_count, "close call");
  close_calls[close_calls_count++] = fd;
  return 0;
}

ssize_t fixture_posix_read(int fd, void *buffer, size_t count) {
  mock_posix_read_step_t step;

  require_capacity(read_calls_count, "read call");
  read_calls[read_calls_count++] = (mock_posix_io_call_t){
    .fd = fd,
    .count = count,
  };
  if (read_step_index >= read_step_count) {
    errno = EAGAIN;
    return -1;
  }
  step = read_steps[read_step_index++];
  if (step.result > 0) {
    size_t copy_length = (size_t)step.result;
    if (copy_length > count) copy_length = count;
    if (copy_length > step.data_length) copy_length = step.data_length;
    memcpy(buffer, step.data, copy_length);
    return (ssize_t)copy_length;
  }
  if (step.result < 0) errno = step.error_number;
  return (ssize_t)step.result;
}

ssize_t fixture_posix_write(
  int fd,
  const void *buffer,
  size_t count
) {
  (void)buffer;
  if (fd == MOCK_POSIX_WAKE_WRITE_FD) {
    ++wake_write_calls;
    if (wake_write_error != 0) {
      errno = wake_write_error;
      return -1;
    }
    return count == 0 ? 0 : 1;
  }
  errno = EBADF;
  return -1;
}

int fixture_posix_poll(
  struct pollfd *fds,
  nfds_t count,
  int timeout_ms
) {
  mock_posix_poll_step_t step;
  mock_posix_poll_call_t *call;
  nfds_t index;

  require_capacity(poll_calls_count, "poll call");
  call = &poll_calls[poll_calls_count++];
  call->descriptor_count = count;
  call->timeout_ms = timeout_ms;
  call->wake_fd = -1;
  call->serial_fd = -1;
  for (index = 0; index < count; ++index) {
    fds[index].revents = 0;
    if (fds[index].fd == MOCK_POSIX_WAKE_READ_FD) {
      call->wake_fd = fds[index].fd;
      call->wake_events = fds[index].events;
    } else {
      call->serial_fd = fds[index].fd;
      call->serial_events = fds[index].events;
    }
  }
  if (poll_step_index >= poll_step_count) {
    errno = EINVAL;
    return -1;
  }
  step = poll_steps[poll_step_index++];
  if (step.signal_number > 0 && step.signal_number < NSIG) {
    const struct sigaction *action = &current_actions[step.signal_number];
    if (
      action->sa_handler != SIG_DFL &&
      action->sa_handler != SIG_IGN &&
      action->sa_handler != NULL
    ) {
      errno = E2BIG;
      action->sa_handler(step.signal_number);
      observed_signal_handler_errno = errno;
    }
  }
  for (index = 0; index < count; ++index) {
    if (fds[index].fd == MOCK_POSIX_WAKE_READ_FD) {
      fds[index].revents = step.wake_revents;
    } else {
      fds[index].revents = step.serial_revents;
    }
  }
  if (step.result < 0) errno = step.error_number;
  return step.result;
}

int fixture_posix_pipe2(int pipe_fds[2], int flags) {
  ++pipe2_calls;
  recorded_pipe2_flags = flags;
  if (pipe2_error != 0) {
    errno = pipe2_error;
    return -1;
  }
  pipe_fds[0] = MOCK_POSIX_WAKE_READ_FD;
  pipe_fds[1] = MOCK_POSIX_WAKE_WRITE_FD;
  return 0;
}

int fixture_posix_sigaction(
  int signal_number,
  const struct sigaction *action,
  struct sigaction *old_action
) {
  signal_call_t *call;

  require_capacity(signal_calls_count, "sigaction call");
  call = &signal_calls[signal_calls_count++];
  call->signal_number = signal_number;
  call->is_restore =
    action != NULL && action->sa_handler == SIG_DFL;
  call->flags = action == NULL ? 0 : action->sa_flags;
  if (
    failing_sigaction_call != 0 &&
    signal_calls_count == failing_sigaction_call
  ) {
    errno = sigaction_error;
    return -1;
  }
  if (signal_number <= 0 || signal_number >= NSIG) {
    errno = EINVAL;
    return -1;
  }
  if (old_action != NULL) {
    *old_action = current_actions[signal_number];
  }
  if (action != NULL) {
    current_actions[signal_number] = *action;
  }
  return 0;
}

int fixture_posix_sigemptyset(sigset_t *set) {
  return sigemptyset(set);
}

int fixture_posix_tcgetattr(int fd, struct termios *attributes) {
  (void)fd;
  ++tcgetattr_calls;
  if (
    failing_tcgetattr_call != 0 &&
    tcgetattr_calls == failing_tcgetattr_call
  ) {
    errno = tcgetattr_error;
    return -1;
  }
  initialize_termios(attributes);
  return 0;
}

int fixture_posix_tcsetattr(
  int fd,
  int optional_actions,
  const struct termios *attributes
) {
  ++tcsetattr_calls;
  configured_fd = fd;
  configured_action = optional_actions;
  configured_attributes = *attributes;
  if (
    failing_tcsetattr_call != 0 &&
    tcsetattr_calls == failing_tcsetattr_call
  ) {
    errno = tcsetattr_error;
    return -1;
  }
  return 0;
}

size_t mock_posix_open_call_count(void) {
  return open_calls_count;
}

const char *mock_posix_open_path(size_t index) {
  return index < open_calls_count ? open_calls[index].path : "";
}

int mock_posix_open_flags(size_t index) {
  return index < open_calls_count ? open_calls[index].flags : 0;
}

size_t mock_posix_poll_call_count(void) {
  return poll_calls_count;
}

mock_posix_poll_call_t mock_posix_poll_call(size_t index) {
  if (index < poll_calls_count) return poll_calls[index];
  return (mock_posix_poll_call_t){0};
}

size_t mock_posix_read_call_count(void) {
  return read_calls_count;
}

mock_posix_io_call_t mock_posix_read_call(size_t index) {
  if (index < read_calls_count) return read_calls[index];
  return (mock_posix_io_call_t){0};
}

size_t mock_posix_wake_write_count(void) {
  return wake_write_calls;
}

size_t mock_posix_close_call_count(void) {
  return close_calls_count;
}

int mock_posix_close_fd(size_t index) {
  return index < close_calls_count ? close_calls[index] : -1;
}

size_t mock_posix_sigaction_call_count(void) {
  return signal_calls_count;
}

int mock_posix_sigaction_signal(size_t index) {
  return index < signal_calls_count
    ? signal_calls[index].signal_number
    : -1;
}

bool mock_posix_sigaction_is_restore(size_t index) {
  return index < signal_calls_count && signal_calls[index].is_restore;
}

int mock_posix_sigaction_flags(size_t index) {
  return index < signal_calls_count ? signal_calls[index].flags : 0;
}

int mock_posix_signal_handler_errno(void) {
  return observed_signal_handler_errno;
}

size_t mock_posix_tcgetattr_call_count(void) {
  return tcgetattr_calls;
}

size_t mock_posix_tcsetattr_call_count(void) {
  return tcsetattr_calls;
}

int mock_posix_tcsetattr_fd(void) {
  return configured_fd;
}

int mock_posix_tcsetattr_action(void) {
  return configured_action;
}

const struct termios *mock_posix_tcsetattr_attributes(void) {
  return &configured_attributes;
}

size_t mock_posix_pipe2_call_count(void) {
  return pipe2_calls;
}

int mock_posix_pipe2_flags(void) {
  return recorded_pipe2_flags;
}
