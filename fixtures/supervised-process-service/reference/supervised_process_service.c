#include "supervised_service.h"
#include "supervisor_os.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define REQUEST_TYPE 0x51u
#define ACK_TYPE 0x41u
#define ACK_ACCEPTED 0u
#define ACK_REJECTED 1u
#define REQUEST_HEADER_BYTES 6u
#define ACK_BYTES 6u

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t signal_write_fd = -1;

static void handle_stop_signal(int signal_number) {
  const int saved_errno = errno;
  const uint8_t byte = (uint8_t)signal_number;
  const int fd = (int)signal_write_fd;

  stop_requested = 1;
  if (fd >= 0) {
    (void)write(fd, &byte, 1u);
  }
  errno = saved_errno;
}

static void encode_u32_le(uint8_t *output, uint32_t value) {
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8);
  output[2] = (uint8_t)(value >> 16);
  output[3] = (uint8_t)(value >> 24);
}

static uint32_t decode_u32_le(const uint8_t *input) {
  return (uint32_t)input[0] |
    ((uint32_t)input[1] << 8) |
    ((uint32_t)input[2] << 16) |
    ((uint32_t)input[3] << 24);
}

static bool valid_arguments(
  const char *worker_path,
  const supervised_service_message_t *messages,
  size_t message_count
) {
  size_t index;

  if (
    worker_path == NULL ||
    worker_path[0] == '\0' ||
    messages == NULL ||
    message_count == 0u ||
    message_count > SUPERVISED_SERVICE_MAX_MESSAGES
  ) {
    return false;
  }
  for (index = 0; index < message_count; ++index) {
    size_t previous;

    if (
      messages[index].sequence == 0u ||
      messages[index].length == 0u ||
      messages[index].length > SUPERVISED_SERVICE_PAYLOAD_BYTES
    ) {
      return false;
    }
    for (previous = 0; previous < index; ++previous) {
      if (messages[index].sequence == messages[previous].sequence) {
        return false;
      }
    }
  }
  return true;
}

static void close_if_open(int *fd) {
  if (*fd >= 0) {
    (void)close(*fd);
    *fd = -1;
  }
}

static int poll_pidfd(int pidfd, int timeout_ms) {
  struct pollfd descriptor = {
    .fd = pidfd,
    .events = POLLIN,
    .revents = 0,
  };
  const int result = poll(&descriptor, 1u, timeout_ms);

  if (result <= 0) return result;
  if ((descriptor.revents & POLLIN) != 0) return 1;
  errno = EIO;
  return -1;
}

static bool reap_child(pid_t pid) {
  int status = 0;
  const pid_t result = waitpid(pid, &status, WNOHANG);

  return result == pid;
}

static bool stop_child(pid_t pid, int pidfd, bool child_ready) {
  int wait_result;
  bool cleanup_ok = true;

  if (!child_ready) {
    if (kill(pid, SIGTERM) != 0 && errno != ESRCH) cleanup_ok = false;
    wait_result = poll_pidfd(pidfd, SUPERVISED_SERVICE_SHUTDOWN_GRACE_MS);
    if (wait_result < 0) cleanup_ok = false;
    if (wait_result <= 0) {
      if (kill(pid, SIGKILL) != 0 && errno != ESRCH) cleanup_ok = false;
      wait_result = poll_pidfd(pidfd, SUPERVISED_SERVICE_KILL_TIMEOUT_MS);
      if (wait_result <= 0) cleanup_ok = false;
    }
  }
  if (!reap_child(pid)) cleanup_ok = false;
  return cleanup_ok;
}

static int poll_restart_delay(int wake_fd, int timeout_ms) {
  struct pollfd descriptor = {
    .fd = wake_fd,
    .events = POLLIN,
    .revents = 0,
  };
  const int result = poll(&descriptor, 1u, timeout_ms);

  if (result <= 0) return result;
  if (stop_requested != 0) return result;
  errno = EIO;
  return -1;
}

typedef struct {
  short wake_events;
  short pid_events;
  short channel_events;
} worker_events_t;

static int poll_worker(
  int wake_fd,
  int pidfd,
  int channel_fd,
  short channel_interest,
  int timeout_ms,
  worker_events_t *events
) {
  struct pollfd descriptors[3] = {
    {
      .fd = wake_fd,
      .events = POLLIN,
      .revents = 0,
    },
    {
      .fd = pidfd,
      .events = POLLIN,
      .revents = 0,
    },
    {
      .fd = channel_fd,
      .events = channel_interest,
      .revents = 0,
    },
  };
  const int result = poll(descriptors, 3u, timeout_ms);

  if (result >= 0) {
    events->wake_events = descriptors[0].revents;
    events->pid_events = descriptors[1].revents;
    events->channel_events = descriptors[2].revents;
  }
  return result;
}

static bool worker_exited(const worker_events_t *events) {
  return (events->pid_events & POLLIN) != 0;
}

static bool pidfd_failed(const worker_events_t *events) {
  const short terminal = POLLERR | POLLHUP | POLLNVAL;

  return (events->pid_events & terminal) != 0;
}

static bool channel_failed(const worker_events_t *events) {
  const short terminal = POLLERR | POLLHUP | POLLNVAL;

  return (events->channel_events & terminal) != 0;
}

static size_t build_request(
  uint8_t *frame,
  const supervised_service_message_t *message
) {
  frame[0] = REQUEST_TYPE;
  encode_u32_le(&frame[1], message->sequence);
  frame[5] = (uint8_t)message->length;
  memcpy(&frame[REQUEST_HEADER_BYTES], message->payload, message->length);
  return REQUEST_HEADER_BYTES + message->length;
}

static bool retryable_channel_error(int error_number) {
  return error_number == EAGAIN ||
    error_number == EWOULDBLOCK ||
    error_number == EINTR ||
    error_number == EPIPE ||
    error_number == ECONNRESET ||
    error_number == ENOTCONN;
}

static int send_request(
  int wake_fd,
  int pidfd,
  int channel_fd,
  const supervised_service_message_t *message
) {
  uint8_t frame[REQUEST_HEADER_BYTES + SUPERVISED_SERVICE_PAYLOAD_BYTES];
  worker_events_t events = {0};
  const size_t frame_length = build_request(frame, message);
  const int poll_result = poll_worker(
    wake_fd,
    pidfd,
    channel_fd,
    POLLOUT,
    SUPERVISED_SERVICE_ACK_TIMEOUT_MS,
    &events
  );
  ssize_t sent;

  if (stop_requested != 0) return 1;
  if (poll_result < 0) {
    if (errno == EINTR && stop_requested != 0) return 1;
    return -1;
  }
  if (poll_result == 0) return 2;
  if ((events.wake_events & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) != 0) {
    errno = EIO;
    return -1;
  }
  if (worker_exited(&events)) return 5;
  if (pidfd_failed(&events)) {
    errno = EIO;
    return -1;
  }
  if (channel_failed(&events) || (events.channel_events & POLLOUT) == 0) {
    return 2;
  }
  sent = send(channel_fd, frame, frame_length, MSG_NOSIGNAL);
  if (sent < 0 && retryable_channel_error(errno)) return 2;
  if (sent != (ssize_t)frame_length) {
    if (sent >= 0) errno = EIO;
    return -1;
  }
  return 0;
}

static int receive_ack(
  int wake_fd,
  int pidfd,
  int channel_fd,
  uint32_t expected_sequence
) {
  uint8_t ack[ACK_BYTES];
  worker_events_t events = {0};
  const int poll_result = poll_worker(
    wake_fd,
    pidfd,
    channel_fd,
    POLLIN,
    SUPERVISED_SERVICE_ACK_TIMEOUT_MS,
    &events
  );
  ssize_t received;

  if (stop_requested != 0) return 1;
  if (poll_result < 0) {
    if (errno == EINTR && stop_requested != 0) return 1;
    return -1;
  }
  if (poll_result == 0) return 2;
  if ((events.wake_events & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) != 0) {
    errno = EIO;
    return -1;
  }
  if (worker_exited(&events)) return 5;
  if (pidfd_failed(&events)) {
    errno = EIO;
    return -1;
  }
  if (channel_failed(&events)) return 2;
  if ((events.channel_events & POLLIN) == 0) {
    errno = EIO;
    return -1;
  }
  received = recv(channel_fd, ack, sizeof(ack), MSG_TRUNC);
  if (received < 0 && retryable_channel_error(errno)) return 2;
  if (received <= 0) return 2;
  if (
    received != (ssize_t)sizeof(ack) ||
    ack[0] != ACK_TYPE ||
    decode_u32_le(&ack[1]) != expected_sequence ||
    (ack[5] != ACK_ACCEPTED && ack[5] != ACK_REJECTED)
  ) {
    return 3;
  }
  return ack[5] == ACK_ACCEPTED ? 0 : 4;
}

static bool retryable_spawn_error(int error_number) {
  return error_number == EAGAIN || error_number == ENOMEM;
}

supervised_service_result_t supervised_service_run(
  const char *worker_path,
  const supervised_service_message_t *messages,
  size_t message_count
) {
  struct sigaction action = {0};
  struct sigaction old_interrupt_action;
  struct sigaction old_terminate_action;
  int wake_pipe[2] = {-1, -1};
  pid_t child_pid = -1;
  int pidfd = -1;
  int channel_fd = -1;
  size_t message_index = 0u;
  size_t restart_count = 0u;
  int restart_delay_ms = SUPERVISED_SERVICE_RESTART_INITIAL_MS;
  bool interrupt_installed = false;
  bool terminate_installed = false;
  bool wait_before_spawn = false;
  bool child_ready = false;
  supervised_service_result_t result = SUPERVISED_SERVICE_OK;

  if (!valid_arguments(worker_path, messages, message_count)) {
    return SUPERVISED_SERVICE_INVALID_ARGUMENT;
  }
  if (pipe2(wake_pipe, O_NONBLOCK | O_CLOEXEC) != 0) {
    return SUPERVISED_SERVICE_OS_ERROR;
  }
  stop_requested = 0;
  signal_write_fd = (sig_atomic_t)wake_pipe[1];
  action.sa_handler = handle_stop_signal;
  action.sa_flags = 0;
  if (sigemptyset(&action.sa_mask) != 0) {
    result = SUPERVISED_SERVICE_OS_ERROR;
    goto cleanup;
  }
  if (sigaction(SIGINT, &action, &old_interrupt_action) != 0) {
    result = SUPERVISED_SERVICE_OS_ERROR;
    goto cleanup;
  }
  interrupt_installed = true;
  if (sigaction(SIGTERM, &action, &old_terminate_action) != 0) {
    result = SUPERVISED_SERVICE_OS_ERROR;
    goto cleanup;
  }
  terminate_installed = true;

  while (message_index < message_count && stop_requested == 0) {
    int operation_result;

    if (child_pid < 0) {
      if (wait_before_spawn) {
        const int wait_result = poll_restart_delay(
          wake_pipe[0],
          restart_delay_ms
        );

        if (stop_requested != 0) break;
        if (wait_result < 0) {
          result = SUPERVISED_SERVICE_OS_ERROR;
          break;
        }
        if (wait_result > 0) {
          result = SUPERVISED_SERVICE_OS_ERROR;
          break;
        }
        if (restart_delay_ms < SUPERVISED_SERVICE_RESTART_MAX_MS) {
          restart_delay_ms *= 2;
          if (restart_delay_ms > SUPERVISED_SERVICE_RESTART_MAX_MS) {
            restart_delay_ms = SUPERVISED_SERVICE_RESTART_MAX_MS;
          }
        }
        wait_before_spawn = false;
      }
      if (
        supervisor_os_spawn_worker(
          worker_path,
          &child_pid,
          &pidfd,
          &channel_fd
        ) != 0
      ) {
        const int spawn_error = errno;

        child_pid = -1;
        pidfd = -1;
        channel_fd = -1;
        if (!retryable_spawn_error(spawn_error)) {
          result = SUPERVISED_SERVICE_OS_ERROR;
          break;
        }
        if (restart_count == SUPERVISED_SERVICE_MAX_RESTARTS) {
          result = SUPERVISED_SERVICE_RESTART_LIMIT;
          break;
        }
        ++restart_count;
        wait_before_spawn = true;
        continue;
      }
      child_ready = false;
    }

    operation_result = send_request(
      wake_pipe[0],
      pidfd,
      channel_fd,
      &messages[message_index]
    );
    if (operation_result == 0) {
      operation_result = receive_ack(
        wake_pipe[0],
        pidfd,
        channel_fd,
        messages[message_index].sequence
      );
    }
    if (operation_result == 0) {
      ++message_index;
      restart_count = 0u;
      restart_delay_ms = SUPERVISED_SERVICE_RESTART_INITIAL_MS;
      continue;
    }
    if (operation_result == 1) break;
    if (operation_result < 0) {
      result = SUPERVISED_SERVICE_OS_ERROR;
      break;
    }
    if (operation_result == 3) {
      result = SUPERVISED_SERVICE_PROTOCOL_ERROR;
      break;
    }
    if (operation_result == 4) {
      result = SUPERVISED_SERVICE_WORKER_REJECTED;
      break;
    }

    child_ready = operation_result == 5;
    {
      const bool child_stopped = stop_child(child_pid, pidfd, child_ready);

      child_pid = -1;
      close_if_open(&channel_fd);
      close_if_open(&pidfd);
      if (!child_stopped) {
        result = SUPERVISED_SERVICE_OS_ERROR;
        break;
      }
    }
    if (restart_count == SUPERVISED_SERVICE_MAX_RESTARTS) {
      result = SUPERVISED_SERVICE_RESTART_LIMIT;
      break;
    }
    ++restart_count;
    wait_before_spawn = true;
  }

cleanup:
  if (child_pid >= 0) {
    if (!stop_child(child_pid, pidfd, child_ready) &&
        result == SUPERVISED_SERVICE_OK) {
      result = SUPERVISED_SERVICE_OS_ERROR;
    }
  }
  close_if_open(&channel_fd);
  close_if_open(&pidfd);
  signal_write_fd = -1;
  if (terminate_installed) {
    (void)sigaction(SIGTERM, &old_terminate_action, NULL);
  }
  if (interrupt_installed) {
    (void)sigaction(SIGINT, &old_interrupt_action, NULL);
  }
  close_if_open(&wake_pipe[1]);
  close_if_open(&wake_pipe[0]);
  stop_requested = 0;
  return result;
}
