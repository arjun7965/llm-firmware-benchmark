#include "mock_posix_serial.h"
#include "serial_service.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

typedef struct {
  uint8_t bytes[256];
  size_t length;
  size_t calls;
  bool accept;
} capture_t;

static bool capture_bytes(
  const uint8_t *data,
  size_t length,
  void *context
) {
  capture_t *capture = context;

  if (
    capture == NULL ||
    data == NULL ||
    length == 0 ||
    length > sizeof(capture->bytes) - capture->length
  ) {
    return false;
  }
  memcpy(capture->bytes + capture->length, data, length);
  capture->length += length;
  ++capture->calls;
  return capture->accept;
}

static bool fail_check(
  const char *test_name,
  const char *expression,
  int line
) {
  fprintf(
    stderr,
    "not ok - %s:%d: %s\n",
    test_name,
    line,
    expression
  );
  return false;
}

#define CHECK(expression) \
  do { \
    if (!(expression)) { \
      return fail_check(__func__, #expression, __LINE__); \
    } \
  } while (false)

static void queue_signal(int signal_number) {
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = -1,
    .error_number = EINTR,
    .signal_number = signal_number,
  });
}

static void queue_serial_events(short events) {
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = 1,
    .serial_revents = events,
  });
}

static bool closed_fd(int fd) {
  size_t index;

  for (index = 0; index < mock_posix_close_call_count(); ++index) {
    if (mock_posix_close_fd(index) == fd) return true;
  }
  return false;
}

static bool test_invalid_arguments_have_no_os_effects(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  CHECK(
    serial_service_run(NULL, capture_bytes, &capture) ==
    SERIAL_SERVICE_INVALID_ARGUMENT
  );
  CHECK(
    serial_service_run("", capture_bytes, &capture) ==
    SERIAL_SERVICE_INVALID_ARGUMENT
  );
  CHECK(
    serial_service_run("/dev/ttyS0", NULL, &capture) ==
    SERIAL_SERVICE_INVALID_ARGUMENT
  );
  CHECK(mock_posix_pipe2_call_count() == 0u);
  CHECK(mock_posix_open_call_count() == 0u);
  CHECK(mock_posix_sigaction_call_count() == 0u);
  return true;
}

static bool test_configures_device_and_restores_process_state(void) {
  const tcflag_t input_clear =
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
  const tcflag_t local_clear = ECHO | ECHONL | ICANON | ISIG | IEXTEN;
  const struct termios *attributes;
  capture_t capture = {.accept = true};
  int expected_open_flags = O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC;

  mock_posix_reset();
  mock_posix_queue_open(20, 0);
  queue_signal(SIGTERM);

  CHECK(
    serial_service_run("/dev/serial/by-id/sensor", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_pipe2_call_count() == 1u);
  CHECK(
    mock_posix_pipe2_flags() == (O_NONBLOCK | O_CLOEXEC)
  );
  CHECK(mock_posix_open_call_count() == 1u);
  CHECK(
    strcmp(
      mock_posix_open_path(0),
      "/dev/serial/by-id/sensor"
    ) == 0
  );
  CHECK(mock_posix_open_flags(0) == expected_open_flags);
  CHECK(mock_posix_tcgetattr_call_count() == 1u);
  CHECK(mock_posix_tcsetattr_call_count() == 1u);
  CHECK(mock_posix_tcsetattr_fd() == 20);
  CHECK(mock_posix_tcsetattr_action() == TCSANOW);
  attributes = mock_posix_tcsetattr_attributes();
  CHECK((attributes->c_iflag & input_clear) == 0);
  CHECK((attributes->c_oflag & OPOST) == 0);
  CHECK((attributes->c_lflag & local_clear) == 0);
  CHECK((attributes->c_cflag & CSIZE) == CS8);
  CHECK(
    (
      attributes->c_cflag &
      (PARENB | PARODD | CSTOPB | CRTSCTS)
    ) == 0
  );
  CHECK((attributes->c_cflag & (CLOCAL | CREAD)) == (CLOCAL | CREAD));
  CHECK(attributes->c_cc[VMIN] == 0);
  CHECK(attributes->c_cc[VTIME] == 0);
  CHECK(cfgetispeed(attributes) == B115200);
  CHECK(cfgetospeed(attributes) == B115200);
  CHECK(mock_posix_wake_write_count() == 1u);
  CHECK(mock_posix_sigaction_call_count() == 4u);
  CHECK(mock_posix_sigaction_signal(0) == SIGINT);
  CHECK(!mock_posix_sigaction_is_restore(0));
  CHECK(mock_posix_sigaction_flags(0) == 0);
  CHECK(mock_posix_sigaction_signal(1) == SIGTERM);
  CHECK(!mock_posix_sigaction_is_restore(1));
  CHECK(mock_posix_sigaction_flags(1) == 0);
  CHECK(mock_posix_sigaction_signal(2) == SIGTERM);
  CHECK(mock_posix_sigaction_is_restore(2));
  CHECK(mock_posix_sigaction_signal(3) == SIGINT);
  CHECK(mock_posix_sigaction_is_restore(3));
  CHECK(mock_posix_signal_handler_errno() == E2BIG);
  CHECK(mock_posix_close_call_count() == 3u);
  CHECK(mock_posix_close_fd(0) == 20);
  CHECK(mock_posix_close_fd(1) == MOCK_POSIX_WAKE_WRITE_FD);
  CHECK(mock_posix_close_fd(2) == MOCK_POSIX_WAKE_READ_FD);
  return true;
}

static bool test_delivers_bounded_reads_exactly_once(void) {
  const uint8_t first[] = {0x10, 0x20, 0x30};
  const uint8_t second[] = {0x40, 0x50};
  const uint8_t expected[] = {0x10, 0x20, 0x30, 0x40, 0x50};
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(30, 0);
  queue_serial_events(POLLIN);
  mock_posix_queue_read_data(first, sizeof(first));
  queue_serial_events(POLLIN);
  mock_posix_queue_read_data(second, sizeof(second));
  queue_signal(SIGINT);

  CHECK(
    serial_service_run("/dev/ttyUSB0", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(capture.calls == 2u);
  CHECK(capture.length == sizeof(expected));
  CHECK(memcmp(capture.bytes, expected, sizeof(expected)) == 0);
  CHECK(mock_posix_read_call_count() == 2u);
  CHECK(mock_posix_read_call(0).fd == 30);
  CHECK(
    mock_posix_read_call(0).count == SERIAL_SERVICE_BUFFER_BYTES
  );
  CHECK(
    mock_posix_read_call(1).count == SERIAL_SERVICE_BUFFER_BYTES
  );
  CHECK(mock_posix_open_call_count() == 1u);
  return true;
}

static bool test_reconnects_with_capped_resetting_backoff(void) {
  const int expected_timeouts[] = {100, 200, -1, 100, -1};
  capture_t capture = {.accept = true};
  size_t index;

  mock_posix_reset();
  mock_posix_queue_open(-1, ENOENT);
  mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
  mock_posix_queue_open(-1, EBUSY);
  mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
  mock_posix_queue_open(40, 0);
  queue_serial_events(POLLHUP);
  mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
  mock_posix_queue_open(41, 0);
  queue_signal(SIGTERM);

  CHECK(
    serial_service_run("/dev/ttyACM0", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_open_call_count() == 4u);
  for (index = 0; index < 4u; ++index) {
    CHECK(strcmp(mock_posix_open_path(index), "/dev/ttyACM0") == 0);
  }
  CHECK(mock_posix_tcgetattr_call_count() == 2u);
  CHECK(mock_posix_tcsetattr_call_count() == 2u);
  CHECK(mock_posix_poll_call_count() == 5u);
  for (index = 0; index < 5u; ++index) {
    CHECK(
      mock_posix_poll_call(index).timeout_ms ==
      expected_timeouts[index]
    );
  }
  CHECK(closed_fd(40));
  CHECK(closed_fd(41));
  return true;
}

static bool test_reconnect_backoff_stops_at_maximum(void) {
  const int expected_timeouts[] = {
    100,
    200,
    400,
    800,
    1600,
    1600,
    1600,
  };
  const int errors[] = {
    ENOENT,
    ENODEV,
    ENXIO,
    EIO,
    EBUSY,
    ENOENT,
    ENODEV,
  };
  capture_t capture = {.accept = true};
  size_t index;

  mock_posix_reset();
  for (index = 0; index < 7u; ++index) {
    mock_posix_queue_open(-1, errors[index]);
    if (index + 1u == 7u) {
      queue_signal(SIGTERM);
    } else {
      mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
    }
  }

  CHECK(
    serial_service_run("/dev/ttyS4", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_open_call_count() == 7u);
  CHECK(mock_posix_poll_call_count() == 7u);
  for (index = 0; index < 7u; ++index) {
    CHECK(
      mock_posix_poll_call(index).timeout_ms ==
      expected_timeouts[index]
    );
  }
  return true;
}

static bool test_retries_recoverable_configuration_failure(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(50, 0);
  mock_posix_fail_tcgetattr_call(1u, EIO);
  mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
  mock_posix_queue_open(51, 0);
  queue_signal(SIGTERM);

  CHECK(
    serial_service_run("/dev/ttyAMA0", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_open_call_count() == 2u);
  CHECK(mock_posix_tcgetattr_call_count() == 2u);
  CHECK(mock_posix_tcsetattr_call_count() == 1u);
  CHECK(mock_posix_poll_call(0).timeout_ms == 100);
  CHECK(closed_fd(50));
  CHECK(closed_fd(51));
  return true;
}

static bool test_transient_read_and_poll_errors_keep_connection(void) {
  const uint8_t payload[] = {0x5a};
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(60, 0);
  queue_serial_events(POLLIN);
  mock_posix_queue_read_result(-1, EAGAIN);
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = -1,
    .error_number = EINTR,
  });
  queue_serial_events(POLLIN);
  mock_posix_queue_read_data(payload, sizeof(payload));
  queue_signal(SIGTERM);

  CHECK(
    serial_service_run("/dev/ttyS1", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_open_call_count() == 1u);
  CHECK(mock_posix_read_call_count() == 2u);
  CHECK(capture.calls == 1u);
  CHECK(capture.length == 1u);
  CHECK(capture.bytes[0] == payload[0]);
  return true;
}

static bool test_eof_and_device_error_reconnect(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(80, 0);
  queue_serial_events(POLLIN);
  mock_posix_queue_read_result(0, 0);
  mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
  mock_posix_queue_open(81, 0);
  queue_serial_events(POLLIN);
  mock_posix_queue_read_result(-1, ENODEV);
  mock_posix_queue_poll((mock_posix_poll_step_t){.result = 0});
  mock_posix_queue_open(82, 0);
  queue_signal(SIGTERM);

  CHECK(
    serial_service_run("/dev/ttyS2", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_open_call_count() == 3u);
  CHECK(mock_posix_poll_call(1).timeout_ms == 100);
  CHECK(mock_posix_poll_call(3).timeout_ms == 100);
  CHECK(closed_fd(80));
  CHECK(closed_fd(81));
  CHECK(closed_fd(82));
  return true;
}

static bool test_stop_has_priority_over_ready_serial_data(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(90, 0);
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = 2,
    .serial_revents = POLLIN,
    .wake_revents = POLLIN,
    .signal_number = SIGINT,
  });

  CHECK(
    serial_service_run("/dev/ttyS3", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(capture.calls == 0u);
  CHECK(mock_posix_read_call_count() == 0u);
  CHECK(mock_posix_wake_write_count() == 1u);
  return true;
}

static bool test_consumer_failure_is_terminal_and_cleans_up(void) {
  const uint8_t payload[] = {1, 2, 3, 4};
  capture_t capture = {.accept = false};

  mock_posix_reset();
  mock_posix_queue_open(100, 0);
  queue_serial_events(POLLIN);
  mock_posix_queue_read_data(payload, sizeof(payload));

  CHECK(
    serial_service_run("/dev/ttyS5", capture_bytes, &capture) ==
    SERIAL_SERVICE_CONSUMER_ERROR
  );
  CHECK(capture.calls == 1u);
  CHECK(closed_fd(100));
  CHECK(closed_fd(MOCK_POSIX_WAKE_READ_FD));
  CHECK(closed_fd(MOCK_POSIX_WAKE_WRITE_FD));
  CHECK(mock_posix_sigaction_call_count() == 4u);
  return true;
}

static bool test_fatal_device_and_poll_errors_do_not_retry(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(-1, EACCES);
  CHECK(
    serial_service_run("/dev/ttyS6", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(mock_posix_open_call_count() == 1u);
  CHECK(mock_posix_poll_call_count() == 0u);

  mock_posix_reset();
  mock_posix_queue_open(110, 0);
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = -1,
    .error_number = EBADF,
  });
  CHECK(
    serial_service_run("/dev/ttyS6", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(mock_posix_open_call_count() == 1u);
  CHECK(closed_fd(110));

  mock_posix_reset();
  mock_posix_queue_open(111, 0);
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = 1,
    .wake_revents = POLLERR,
  });
  CHECK(
    serial_service_run("/dev/ttyS6", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(closed_fd(111));
  CHECK(mock_posix_poll_call_count() == 1u);

  mock_posix_reset();
  mock_posix_queue_open(-1, ENOENT);
  mock_posix_queue_poll((mock_posix_poll_step_t){
    .result = 1,
    .wake_revents = POLLHUP,
  });
  CHECK(
    serial_service_run("/dev/ttyS6", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(mock_posix_open_call_count() == 1u);
  return true;
}

static bool test_setup_failures_release_acquired_resources(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_fail_pipe2(EMFILE);
  CHECK(
    serial_service_run("/dev/ttyS7", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(mock_posix_close_call_count() == 0u);
  CHECK(mock_posix_sigaction_call_count() == 0u);

  mock_posix_reset();
  mock_posix_fail_sigaction_call(1u, EINVAL);
  CHECK(
    serial_service_run("/dev/ttyS7", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(mock_posix_sigaction_call_count() == 1u);
  CHECK(closed_fd(MOCK_POSIX_WAKE_READ_FD));
  CHECK(closed_fd(MOCK_POSIX_WAKE_WRITE_FD));

  mock_posix_reset();
  mock_posix_fail_sigaction_call(2u, EINVAL);
  CHECK(
    serial_service_run("/dev/ttyS7", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(mock_posix_sigaction_call_count() == 3u);
  CHECK(mock_posix_sigaction_signal(2) == SIGINT);
  CHECK(mock_posix_sigaction_is_restore(2));

  mock_posix_reset();
  mock_posix_queue_open(120, 0);
  mock_posix_fail_tcsetattr_call(1u, EINVAL);
  CHECK(
    serial_service_run("/dev/ttyS7", capture_bytes, &capture) ==
    SERIAL_SERVICE_OS_ERROR
  );
  CHECK(closed_fd(120));
  CHECK(mock_posix_sigaction_call_count() == 4u);
  return true;
}

static bool test_signal_handler_tolerates_full_wake_pipe(void) {
  capture_t capture = {.accept = true};

  mock_posix_reset();
  mock_posix_queue_open(130, 0);
  mock_posix_fail_wake_write(EAGAIN);
  queue_signal(SIGTERM);

  CHECK(
    serial_service_run("/dev/ttyS8", capture_bytes, &capture) ==
    SERIAL_SERVICE_OK
  );
  CHECK(mock_posix_wake_write_count() == 1u);
  CHECK(closed_fd(130));
  return true;
}

int main(void) {
  typedef bool (*test_function_t)(void);
  static const struct {
    const char *name;
    test_function_t function;
  } tests[] = {
    {
      "invalid arguments have no OS effects",
      test_invalid_arguments_have_no_os_effects,
    },
    {
      "configure device and restore process state",
      test_configures_device_and_restores_process_state,
    },
    {
      "deliver bounded reads exactly once",
      test_delivers_bounded_reads_exactly_once,
    },
    {
      "reconnect with capped resetting backoff",
      test_reconnects_with_capped_resetting_backoff,
    },
    {
      "reconnect backoff stops at maximum",
      test_reconnect_backoff_stops_at_maximum,
    },
    {
      "retry recoverable configuration failure",
      test_retries_recoverable_configuration_failure,
    },
    {
      "transient errors keep connection",
      test_transient_read_and_poll_errors_keep_connection,
    },
    {
      "EOF and device error reconnect",
      test_eof_and_device_error_reconnect,
    },
    {
      "stop has priority",
      test_stop_has_priority_over_ready_serial_data,
    },
    {
      "consumer failure cleans up",
      test_consumer_failure_is_terminal_and_cleans_up,
    },
    {
      "fatal OS errors do not retry",
      test_fatal_device_and_poll_errors_do_not_retry,
    },
    {
      "setup failures release resources",
      test_setup_failures_release_acquired_resources,
    },
    {
      "full wake pipe remains graceful",
      test_signal_handler_tolerates_full_wake_pipe,
    },
  };
  size_t index;

  for (index = 0; index < sizeof(tests) / sizeof(tests[0]); ++index) {
    if (!tests[index].function()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}
