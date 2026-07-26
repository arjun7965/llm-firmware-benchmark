#include "serial_service.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <termios.h>
#include <unistd.h>

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

static bool retryable_device_error(int error_number) {
  return error_number == ENOENT ||
    error_number == ENODEV ||
    error_number == ENXIO ||
    error_number == EIO ||
    error_number == EBUSY;
}

static bool configure_device(int fd) {
  struct termios attributes;

  if (tcgetattr(fd, &attributes) != 0) return false;
  attributes.c_iflag &= (tcflag_t)~(
    IGNBRK |
    BRKINT |
    PARMRK |
    ISTRIP |
    INLCR |
    IGNCR |
    ICRNL |
    IXON |
    IXOFF |
    IXANY
  );
  attributes.c_oflag &= (tcflag_t)~OPOST;
  attributes.c_lflag &= (tcflag_t)~(
    ECHO |
    ECHONL |
    ICANON |
    ISIG |
    IEXTEN
  );
  attributes.c_cflag &= (tcflag_t)~(
    CSIZE |
    PARENB |
    PARODD |
    CSTOPB |
    CRTSCTS
  );
  attributes.c_cflag |= CS8 | CLOCAL | CREAD;
  attributes.c_cc[VMIN] = 0;
  attributes.c_cc[VTIME] = 0;
  if (
    cfsetispeed(&attributes, B115200) != 0 ||
    cfsetospeed(&attributes, B115200) != 0
  ) {
    return false;
  }
  return tcsetattr(fd, TCSANOW, &attributes) == 0;
}

static int poll_for_reconnect(int wake_fd, int timeout_ms) {
  struct pollfd descriptor = {
    .fd = wake_fd,
    .events = POLLIN,
    .revents = 0,
  };
  return poll(&descriptor, 1u, timeout_ms);
}

static int poll_connected(
  int wake_fd,
  int serial_fd,
  short *wake_events,
  short *serial_events
) {
  struct pollfd descriptors[2] = {
    {
      .fd = wake_fd,
      .events = POLLIN,
      .revents = 0,
    },
    {
      .fd = serial_fd,
      .events = POLLIN,
      .revents = 0,
    },
  };
  const int result = poll(descriptors, 2u, -1);

  if (result >= 0) {
    *wake_events = descriptors[0].revents;
    *serial_events = descriptors[1].revents;
  }
  return result;
}

static void close_if_open(int *fd) {
  if (*fd >= 0) {
    (void)close(*fd);
    *fd = -1;
  }
}

serial_service_result_t serial_service_run(
  const char *device_path,
  serial_service_consumer_t consumer,
  void *context
) {
  struct sigaction action = {0};
  struct sigaction old_interrupt_action;
  struct sigaction old_terminate_action;
  uint8_t buffer[SERIAL_SERVICE_BUFFER_BYTES];
  int wake_pipe[2] = {-1, -1};
  int serial_fd = -1;
  int reconnect_delay_ms = SERIAL_SERVICE_RECONNECT_INITIAL_MS;
  bool wait_before_open = false;
  bool interrupt_installed = false;
  bool terminate_installed = false;
  serial_service_result_t result = SERIAL_SERVICE_OK;

  if (device_path == NULL || device_path[0] == '\0' || consumer == NULL) {
    return SERIAL_SERVICE_INVALID_ARGUMENT;
  }
  if (pipe2(wake_pipe, O_NONBLOCK | O_CLOEXEC) != 0) {
    return SERIAL_SERVICE_OS_ERROR;
  }

  stop_requested = 0;
  signal_write_fd = (sig_atomic_t)wake_pipe[1];
  action.sa_handler = handle_stop_signal;
  action.sa_flags = 0;
  if (sigemptyset(&action.sa_mask) != 0) {
    result = SERIAL_SERVICE_OS_ERROR;
    goto cleanup;
  }
  if (sigaction(SIGINT, &action, &old_interrupt_action) != 0) {
    result = SERIAL_SERVICE_OS_ERROR;
    goto cleanup;
  }
  interrupt_installed = true;
  if (sigaction(SIGTERM, &action, &old_terminate_action) != 0) {
    result = SERIAL_SERVICE_OS_ERROR;
    goto cleanup;
  }
  terminate_installed = true;

  while (stop_requested == 0) {
    if (serial_fd < 0) {
      if (wait_before_open) {
        const int wait_result = poll_for_reconnect(
          wake_pipe[0],
          reconnect_delay_ms
        );
        if (stop_requested != 0) break;
        if (wait_result < 0) {
          if (errno == EINTR) continue;
          result = SERIAL_SERVICE_OS_ERROR;
          break;
        }
        if (wait_result > 0) {
          result = SERIAL_SERVICE_OS_ERROR;
          break;
        }
        if (reconnect_delay_ms < SERIAL_SERVICE_RECONNECT_MAX_MS) {
          reconnect_delay_ms *= 2;
          if (reconnect_delay_ms > SERIAL_SERVICE_RECONNECT_MAX_MS) {
            reconnect_delay_ms = SERIAL_SERVICE_RECONNECT_MAX_MS;
          }
        }
        wait_before_open = false;
      }

      serial_fd = open(
        device_path,
        O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC
      );
      if (serial_fd < 0) {
        if (!retryable_device_error(errno)) {
          result = SERIAL_SERVICE_OS_ERROR;
          break;
        }
        wait_before_open = true;
        continue;
      }
      if (!configure_device(serial_fd)) {
        const int configure_error = errno;
        close_if_open(&serial_fd);
        if (!retryable_device_error(configure_error)) {
          result = SERIAL_SERVICE_OS_ERROR;
          break;
        }
        wait_before_open = true;
        continue;
      }
      reconnect_delay_ms = SERIAL_SERVICE_RECONNECT_INITIAL_MS;
    }

    {
      short wake_events = 0;
      short serial_events = 0;
      const int poll_result = poll_connected(
        wake_pipe[0],
        serial_fd,
        &wake_events,
        &serial_events
      );
      if (stop_requested != 0) break;
      if (poll_result < 0) {
        if (errno == EINTR) continue;
        result = SERIAL_SERVICE_OS_ERROR;
        break;
      }
      if ((wake_events & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) != 0) {
        result = SERIAL_SERVICE_OS_ERROR;
        break;
      }
      if ((serial_events & POLLIN) != 0) {
        const ssize_t count = read(serial_fd, buffer, sizeof(buffer));
        if (count > 0) {
          if (!consumer(buffer, (size_t)count, context)) {
            result = SERIAL_SERVICE_CONSUMER_ERROR;
            break;
          }
        } else if (count == 0) {
          close_if_open(&serial_fd);
          wait_before_open = true;
          continue;
        } else if (
          errno != EAGAIN &&
          errno != EWOULDBLOCK &&
          errno != EINTR
        ) {
          const int read_error = errno;
          if (!retryable_device_error(read_error)) {
            result = SERIAL_SERVICE_OS_ERROR;
            break;
          }
          close_if_open(&serial_fd);
          wait_before_open = true;
          continue;
        }
      }
      if ((serial_events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        close_if_open(&serial_fd);
        wait_before_open = true;
      }
    }
  }

cleanup:
  close_if_open(&serial_fd);
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
