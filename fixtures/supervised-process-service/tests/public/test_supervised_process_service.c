#include "mock_supervisor_os.h"
#include "supervised_service.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define TEST_ACK_TYPE 0x41u
#define TEST_REQUEST_TYPE 0x51u

#define ASSERT(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "assertion failed at line %d: %s\n", __LINE__, #condition); \
    return false; \
  } \
} while (0)

static supervised_service_message_t message(
  uint32_t sequence,
  const uint8_t *payload,
  size_t length
) {
  supervised_service_message_t value = {
    .sequence = sequence,
    .length = length,
    .payload = {0},
  };

  if (payload != NULL && length <= sizeof(value.payload)) {
    memcpy(value.payload, payload, length);
  }
  return value;
}

static void queue_spawn(pid_t pid, int pidfd, int channel_fd) {
  mock_supervisor_queue_spawn((mock_supervisor_spawn_step_t){
    .result = 0,
    .pid = pid,
    .pidfd = pidfd,
    .channel_fd = channel_fd,
  });
}

static void queue_poll(
  int result,
  int error_number,
  short first,
  short second,
  short third,
  int signal_number
) {
  mock_supervisor_queue_poll((mock_supervisor_poll_step_t){
    .result = result,
    .error_number = error_number,
    .revents = {first, second, third},
    .signal_number = signal_number,
  });
}

static void queue_worker_ready(short events) {
  queue_poll(1, 0, 0, 0, events, 0);
}

static void queue_timeout(void) {
  queue_poll(0, 0, 0, 0, 0, 0);
}

static void queue_child_exit(void) {
  queue_poll(1, 0, POLLIN, 0, 0, 0);
}

static void queue_ack(uint32_t sequence, uint8_t status) {
  uint8_t ack[6] = {
    TEST_ACK_TYPE,
    (uint8_t)sequence,
    (uint8_t)(sequence >> 8),
    (uint8_t)(sequence >> 16),
    (uint8_t)(sequence >> 24),
    status,
  };

  queue_worker_ready(POLLIN);
  mock_supervisor_queue_recv(ack, sizeof(ack), (ssize_t)sizeof(ack), 0);
}

static void queue_successful_message(uint32_t sequence) {
  queue_worker_ready(POLLOUT);
  queue_ack(sequence, 0u);
}

static void queue_graceful_cleanup(void) {
  queue_child_exit();
}

static uint32_t decode_u32(const uint8_t *input) {
  return (uint32_t)input[0] |
    ((uint32_t)input[1] << 8) |
    ((uint32_t)input[2] << 16) |
    ((uint32_t)input[3] << 24);
}

static bool test_invalid_arguments_have_no_os_effects(void) {
  const uint8_t byte = 1u;
  supervised_service_message_t valid = message(1u, &byte, 1u);
  supervised_service_message_t invalid = valid;
  supervised_service_message_t too_many[SUPERVISED_SERVICE_MAX_MESSAGES + 1u];
  size_t index;

  mock_supervisor_reset();
  ASSERT(
    supervised_service_run(NULL, &valid, 1u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  ASSERT(
    supervised_service_run("", &valid, 1u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  ASSERT(
    supervised_service_run("/worker", NULL, 1u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  ASSERT(
    supervised_service_run("/worker", &valid, 0u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  invalid.sequence = 0u;
  ASSERT(
    supervised_service_run("/worker", &invalid, 1u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  invalid = valid;
  invalid.length = 0u;
  ASSERT(
    supervised_service_run("/worker", &invalid, 1u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  invalid.length = SUPERVISED_SERVICE_PAYLOAD_BYTES + 1u;
  ASSERT(
    supervised_service_run("/worker", &invalid, 1u) ==
      SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  for (index = 0u; index < sizeof(too_many) / sizeof(too_many[0]); ++index) {
    too_many[index] = message((uint32_t)index + 1u, &byte, 1u);
  }
  ASSERT(
    supervised_service_run(
      "/worker",
      too_many,
      sizeof(too_many) / sizeof(too_many[0])
    ) == SUPERVISED_SERVICE_INVALID_ARGUMENT
  );
  invalid = valid;
  {
    supervised_service_message_t duplicate[2] = {valid, invalid};

    ASSERT(
      supervised_service_run("/worker", duplicate, 2u) ==
        SUPERVISED_SERVICE_INVALID_ARGUMENT
    );
  }
  ASSERT(mock_supervisor_pipe2_call_count() == 0u);
  ASSERT(mock_supervisor_spawn_call_count() == 0u);
  return true;
}

static bool test_sends_bounded_frames_and_cleans_up(void) {
  const uint8_t first_payload[] = {0x10u, 0x20u, 0x30u};
  const uint8_t second_payload[] = {0xaau, 0xbbu};
  supervised_service_message_t messages[2] = {
    message(0x12345678u, first_payload, sizeof(first_payload)),
    message(9u, second_payload, sizeof(second_payload)),
  };
  mock_supervisor_io_call_t first_send;
  mock_supervisor_io_call_t second_send;
  mock_supervisor_poll_call_t writable_poll;
  mock_supervisor_poll_call_t ack_poll;

  mock_supervisor_reset();
  queue_spawn(1001, 81, 82);
  queue_successful_message(messages[0].sequence);
  queue_successful_message(messages[1].sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/usr/bin/worker", messages, 2u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_pipe2_call_count() == 1u);
  ASSERT(
    mock_supervisor_pipe2_flags() == (O_NONBLOCK | O_CLOEXEC)
  );
  ASSERT(mock_supervisor_spawn_call_count() == 1u);
  ASSERT(strcmp(mock_supervisor_spawn_path(0u), "/usr/bin/worker") == 0);
  ASSERT(mock_supervisor_send_call_count() == 2u);
  first_send = mock_supervisor_send_call(0u);
  second_send = mock_supervisor_send_call(1u);
  ASSERT(first_send.fd == 82);
  ASSERT(first_send.flags == MSG_NOSIGNAL);
  ASSERT(first_send.length == 6u + sizeof(first_payload));
  ASSERT(first_send.data[0] == TEST_REQUEST_TYPE);
  ASSERT(decode_u32(&first_send.data[1]) == messages[0].sequence);
  ASSERT(first_send.data[5] == sizeof(first_payload));
  ASSERT(
    memcmp(&first_send.data[6], first_payload, sizeof(first_payload)) == 0
  );
  ASSERT(second_send.length == 6u + sizeof(second_payload));
  ASSERT(decode_u32(&second_send.data[1]) == messages[1].sequence);
  ASSERT(second_send.data[5] == sizeof(second_payload));
  ASSERT(
    memcmp(&second_send.data[6], second_payload, sizeof(second_payload)) == 0
  );
  ASSERT(mock_supervisor_recv_call_count() == 2u);
  ASSERT(mock_supervisor_recv_call(0u).flags == MSG_TRUNC);
  ASSERT(mock_supervisor_recv_call(0u).length == 6u);
  writable_poll = mock_supervisor_poll_call(0u);
  ack_poll = mock_supervisor_poll_call(1u);
  ASSERT(writable_poll.descriptor_count == 3u);
  ASSERT(writable_poll.timeout_ms == SUPERVISED_SERVICE_ACK_TIMEOUT_MS);
  ASSERT(writable_poll.fds[0] == MOCK_SUPERVISOR_WAKE_READ_FD);
  ASSERT(writable_poll.events[0] == POLLIN);
  ASSERT(writable_poll.fds[1] == 81);
  ASSERT(writable_poll.events[1] == POLLIN);
  ASSERT(writable_poll.fds[2] == 82);
  ASSERT(writable_poll.events[2] == POLLOUT);
  ASSERT(ack_poll.descriptor_count == 3u);
  ASSERT(ack_poll.timeout_ms == SUPERVISED_SERVICE_ACK_TIMEOUT_MS);
  ASSERT(ack_poll.events[2] == POLLIN);
  ASSERT(mock_supervisor_kill_call_count() == 1u);
  ASSERT(mock_supervisor_kill_pid(0u) == 1001);
  ASSERT(mock_supervisor_kill_signal(0u) == SIGTERM);
  ASSERT(mock_supervisor_wait_call_count() == 1u);
  ASSERT(mock_supervisor_wait_pid(0u) == 1001);
  ASSERT(mock_supervisor_wait_options(0u) == WNOHANG);
  ASSERT(mock_supervisor_close_call_count() == 4u);
  ASSERT(mock_supervisor_close_fd(0u) == 82);
  ASSERT(mock_supervisor_close_fd(1u) == 81);
  ASSERT(
    mock_supervisor_close_fd(2u) == MOCK_SUPERVISOR_WAKE_WRITE_FD
  );
  ASSERT(
    mock_supervisor_close_fd(3u) == MOCK_SUPERVISOR_WAKE_READ_FD
  );
  ASSERT(mock_supervisor_sigaction_call_count() == 4u);
  ASSERT(mock_supervisor_sigaction_signal(0u) == SIGINT);
  ASSERT(mock_supervisor_sigaction_signal(1u) == SIGTERM);
  ASSERT(mock_supervisor_sigaction_is_restore(2u));
  ASSERT(mock_supervisor_sigaction_signal(2u) == SIGTERM);
  ASSERT(mock_supervisor_sigaction_is_restore(3u));
  ASSERT(mock_supervisor_sigaction_signal(3u) == SIGINT);
  ASSERT(mock_supervisor_sigaction_flags(0u) == 0);
  ASSERT(mock_supervisor_sigaction_flags(1u) == 0);
  return true;
}

static bool test_restarts_and_resends_unacknowledged_message(void) {
  const uint8_t payload[] = {1u, 2u, 3u, 4u};
  const supervised_service_message_t input =
    message(77u, payload, sizeof(payload));
  mock_supervisor_poll_call_t restart_wait;
  mock_supervisor_io_call_t first_send;
  mock_supervisor_io_call_t second_send;

  mock_supervisor_reset();
  queue_spawn(2001, 91, 92);
  queue_worker_ready(POLLOUT);
  queue_timeout();
  queue_child_exit();
  queue_timeout();
  queue_spawn(2002, 93, 94);
  queue_successful_message(input.sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_spawn_call_count() == 2u);
  ASSERT(mock_supervisor_send_call_count() == 2u);
  first_send = mock_supervisor_send_call(0u);
  second_send = mock_supervisor_send_call(1u);
  ASSERT(first_send.length == second_send.length);
  ASSERT(
    memcmp(first_send.data, second_send.data, first_send.length) == 0
  );
  restart_wait = mock_supervisor_poll_call(3u);
  ASSERT(restart_wait.descriptor_count == 1u);
  ASSERT(restart_wait.fds[0] == MOCK_SUPERVISOR_WAKE_READ_FD);
  ASSERT(restart_wait.timeout_ms == SUPERVISED_SERVICE_RESTART_INITIAL_MS);
  ASSERT(mock_supervisor_kill_call_count() == 2u);
  ASSERT(mock_supervisor_wait_call_count() == 2u);
  return true;
}

static bool test_restart_policy_caps_consecutive_failures(void) {
  const uint8_t payload = 0x5au;
  const supervised_service_message_t input = message(5u, &payload, 1u);
  const int expected_delays[] = {100, 200, 400};
  size_t attempt;
  size_t delay_index = 0u;
  size_t poll_index;

  mock_supervisor_reset();
  for (attempt = 0u; attempt < 4u; ++attempt) {
    queue_spawn(
      (pid_t)(3000 + (int)attempt),
      100 + (int)(attempt * 2u),
      101 + (int)(attempt * 2u)
    );
    queue_worker_ready(POLLOUT);
    queue_timeout();
    queue_child_exit();
    if (attempt < 3u) queue_timeout();
  }

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_RESTART_LIMIT
  );
  ASSERT(mock_supervisor_spawn_call_count() == 4u);
  ASSERT(mock_supervisor_send_call_count() == 4u);
  ASSERT(mock_supervisor_kill_call_count() == 4u);
  ASSERT(mock_supervisor_wait_call_count() == 4u);
  for (poll_index = 0u;
       poll_index < mock_supervisor_poll_call_count();
       ++poll_index) {
    const mock_supervisor_poll_call_t call =
      mock_supervisor_poll_call(poll_index);

    if (
      call.descriptor_count == 1u &&
      call.fds[0] == MOCK_SUPERVISOR_WAKE_READ_FD
    ) {
      ASSERT(delay_index < sizeof(expected_delays) / sizeof(expected_delays[0]));
      ASSERT(call.timeout_ms == expected_delays[delay_index]);
      ++delay_index;
    }
  }
  ASSERT(delay_index == sizeof(expected_delays) / sizeof(expected_delays[0]));
  return true;
}

static bool test_successful_ack_resets_restart_policy(void) {
  const uint8_t payload = 7u;
  supervised_service_message_t inputs[2] = {
    message(10u, &payload, 1u),
    message(20u, &payload, 1u),
  };
  int delays[2] = {0};
  size_t delay_count = 0u;
  size_t poll_index;

  mock_supervisor_reset();
  queue_spawn(4001, 121, 122);
  queue_worker_ready(POLLOUT);
  queue_timeout();
  queue_child_exit();
  queue_timeout();
  queue_spawn(4002, 123, 124);
  queue_successful_message(inputs[0].sequence);
  queue_worker_ready(POLLOUT);
  queue_timeout();
  queue_child_exit();
  queue_timeout();
  queue_spawn(4003, 125, 126);
  queue_successful_message(inputs[1].sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", inputs, 2u) ==
      SUPERVISED_SERVICE_OK
  );
  for (poll_index = 0u;
       poll_index < mock_supervisor_poll_call_count();
       ++poll_index) {
    const mock_supervisor_poll_call_t call =
      mock_supervisor_poll_call(poll_index);

    if (
      call.descriptor_count == 1u &&
      call.fds[0] == MOCK_SUPERVISOR_WAKE_READ_FD
    ) {
      ASSERT(delay_count < 2u);
      delays[delay_count++] = call.timeout_ms;
    }
  }
  ASSERT(delay_count == 2u);
  ASSERT(delays[0] == SUPERVISED_SERVICE_RESTART_INITIAL_MS);
  ASSERT(delays[1] == SUPERVISED_SERVICE_RESTART_INITIAL_MS);
  return true;
}

static bool test_retryable_spawn_and_channel_errors_recover(void) {
  const uint8_t payload = 8u;
  const supervised_service_message_t input = message(33u, &payload, 1u);

  mock_supervisor_reset();
  mock_supervisor_queue_spawn((mock_supervisor_spawn_step_t){
    .result = -1,
    .error_number = EAGAIN,
  });
  queue_timeout();
  queue_spawn(5001, 131, 132);
  queue_worker_ready(POLLOUT);
  mock_supervisor_queue_send(-1, EPIPE);
  queue_child_exit();
  queue_timeout();
  queue_spawn(5002, 133, 134);
  queue_successful_message(input.sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_spawn_call_count() == 3u);
  ASSERT(mock_supervisor_send_call_count() == 2u);
  ASSERT(mock_supervisor_kill_call_count() == 2u);
  return true;
}

static bool test_channel_hangup_restarts_worker(void) {
  const uint8_t payload = 0x41u;
  const supervised_service_message_t input = message(34u, &payload, 1u);

  mock_supervisor_reset();
  queue_spawn(5401, 135, 136);
  queue_worker_ready(POLLHUP);
  queue_child_exit();
  queue_timeout();
  queue_spawn(5402, 137, 138);
  queue_successful_message(input.sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_spawn_call_count() == 2u);
  ASSERT(mock_supervisor_send_call_count() == 1u);
  ASSERT(mock_supervisor_kill_call_count() == 2u);
  ASSERT(mock_supervisor_wait_call_count() == 2u);
  return true;
}

static bool test_pidfd_exit_reaps_without_signaling_dead_child(void) {
  const uint8_t payload = 0x42u;
  const supervised_service_message_t input = message(34u, &payload, 1u);

  mock_supervisor_reset();
  queue_spawn(5501, 135, 136);
  queue_poll(1, 0, 0, POLLIN, 0, 0);
  queue_timeout();
  queue_spawn(5502, 137, 138);
  queue_successful_message(input.sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_spawn_call_count() == 2u);
  ASSERT(mock_supervisor_send_call_count() == 1u);
  ASSERT(mock_supervisor_kill_call_count() == 1u);
  ASSERT(mock_supervisor_kill_pid(0u) == 5502);
  ASSERT(mock_supervisor_wait_call_count() == 2u);
  ASSERT(mock_supervisor_wait_pid(0u) == 5501);
  ASSERT(mock_supervisor_wait_options(0u) == WNOHANG);
  return true;
}

static bool test_terminal_event_wins_over_simultaneous_ack_readiness(void) {
  const uint8_t payload = 0x43u;
  const supervised_service_message_t input = message(35u, &payload, 1u);

  mock_supervisor_reset();
  queue_spawn(5601, 139, 140);
  queue_worker_ready(POLLOUT);
  queue_poll(2, 0, 0, POLLIN, POLLIN, 0);
  queue_timeout();
  queue_spawn(5602, 141, 142);
  queue_successful_message(input.sequence);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_spawn_call_count() == 2u);
  ASSERT(mock_supervisor_send_call_count() == 2u);
  ASSERT(mock_supervisor_recv_call_count() == 1u);
  ASSERT(mock_supervisor_kill_call_count() == 1u);
  ASSERT(mock_supervisor_kill_pid(0u) == 5602);
  ASSERT(mock_supervisor_wait_call_count() == 2u);
  ASSERT(mock_supervisor_wait_pid(0u) == 5601);
  return true;
}

static bool test_protocol_and_worker_rejections_are_terminal(void) {
  const uint8_t payload = 9u;
  const supervised_service_message_t input = message(44u, &payload, 1u);
  uint8_t malformed[7] = {
    TEST_ACK_TYPE,
    44u,
    0u,
    0u,
    0u,
    0u,
    0xffu,
  };

  mock_supervisor_reset();
  queue_spawn(6001, 141, 142);
  queue_worker_ready(POLLOUT);
  queue_worker_ready(POLLIN);
  mock_supervisor_queue_recv(
    malformed,
    sizeof(malformed),
    (ssize_t)sizeof(malformed),
    0
  );
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_PROTOCOL_ERROR
  );
  ASSERT(mock_supervisor_spawn_call_count() == 1u);

  mock_supervisor_reset();
  queue_spawn(6002, 143, 144);
  queue_worker_ready(POLLOUT);
  queue_ack(input.sequence + 1u, 0u);
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_PROTOCOL_ERROR
  );

  mock_supervisor_reset();
  queue_spawn(6003, 145, 146);
  queue_worker_ready(POLLOUT);
  queue_ack(input.sequence, 2u);
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_PROTOCOL_ERROR
  );

  mock_supervisor_reset();
  queue_spawn(6004, 147, 148);
  queue_worker_ready(POLLOUT);
  queue_ack(input.sequence, 1u);
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_WORKER_REJECTED
  );
  ASSERT(mock_supervisor_spawn_call_count() == 1u);
  return true;
}

static bool test_short_send_is_fatal(void) {
  const uint8_t payload[] = {1u, 2u};
  const supervised_service_message_t input =
    message(45u, payload, sizeof(payload));

  mock_supervisor_reset();
  queue_spawn(6501, 149, 150);
  queue_worker_ready(POLLOUT);
  mock_supervisor_queue_send(3, 0);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_spawn_call_count() == 1u);
  ASSERT(mock_supervisor_send_call_count() == 1u);
  ASSERT(mock_supervisor_poll_call_count() == 2u);
  ASSERT(mock_supervisor_kill_call_count() == 1u);
  return true;
}

static bool test_stop_signal_has_priority_and_preserves_errno(void) {
  const uint8_t payload = 10u;
  const supervised_service_message_t input = message(55u, &payload, 1u);

  mock_supervisor_reset();
  queue_spawn(7001, 151, 152);
  queue_worker_ready(POLLOUT);
  queue_poll(1, 0, POLLIN, 0, POLLIN, SIGTERM);
  queue_graceful_cleanup();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_send_call_count() == 1u);
  ASSERT(mock_supervisor_recv_call_count() == 0u);
  ASSERT(mock_supervisor_wake_write_count() == 1u);
  ASSERT(mock_supervisor_signal_handler_errno() == 777);
  ASSERT(mock_supervisor_kill_call_count() == 1u);

  mock_supervisor_reset();
  mock_supervisor_fail_wake_write(EAGAIN);
  queue_spawn(7002, 153, 154);
  queue_poll(-1, EINTR, 0, 0, 0, SIGINT);
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_wake_write_count() == 1u);
  ASSERT(mock_supervisor_signal_handler_errno() == 777);
  return true;
}

static bool test_shutdown_escalates_after_bounded_grace(void) {
  const uint8_t payload = 11u;
  const supervised_service_message_t input = message(66u, &payload, 1u);
  mock_supervisor_poll_call_t grace;
  mock_supervisor_poll_call_t forced;

  mock_supervisor_reset();
  queue_spawn(8001, 161, 162);
  queue_successful_message(input.sequence);
  queue_timeout();
  queue_child_exit();

  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OK
  );
  ASSERT(mock_supervisor_kill_call_count() == 2u);
  ASSERT(mock_supervisor_kill_signal(0u) == SIGTERM);
  ASSERT(mock_supervisor_kill_signal(1u) == SIGKILL);
  grace = mock_supervisor_poll_call(2u);
  forced = mock_supervisor_poll_call(3u);
  ASSERT(grace.descriptor_count == 1u);
  ASSERT(grace.fds[0] == 161);
  ASSERT(grace.timeout_ms == SUPERVISED_SERVICE_SHUTDOWN_GRACE_MS);
  ASSERT(forced.descriptor_count == 1u);
  ASSERT(forced.fds[0] == 161);
  ASSERT(forced.timeout_ms == SUPERVISED_SERVICE_KILL_TIMEOUT_MS);
  ASSERT(mock_supervisor_wait_call_count() == 1u);
  return true;
}

static bool test_shutdown_failures_report_os_error_and_preserve_primary(void) {
  const uint8_t payload = 0x63u;
  const supervised_service_message_t input = message(67u, &payload, 1u);

  mock_supervisor_reset();
  queue_spawn(8101, 163, 164);
  queue_successful_message(input.sequence);
  mock_supervisor_queue_kill(-1, EPERM);
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_kill_call_count() == 1u);
  ASSERT(mock_supervisor_wait_call_count() == 0u);

  mock_supervisor_reset();
  queue_spawn(8102, 165, 166);
  queue_successful_message(input.sequence);
  queue_child_exit();
  mock_supervisor_queue_waitpid(-1, ECHILD, 0);
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_wait_call_count() == 1u);

  mock_supervisor_reset();
  queue_spawn(8103, 167, 168);
  queue_successful_message(input.sequence);
  queue_poll(-1, EBADF, 0, 0, 0, 0);
  queue_child_exit();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_kill_call_count() == 2u);
  ASSERT(mock_supervisor_kill_signal(0u) == SIGTERM);
  ASSERT(mock_supervisor_kill_signal(1u) == SIGKILL);
  ASSERT(mock_supervisor_wait_call_count() == 1u);

  mock_supervisor_reset();
  queue_spawn(8104, 169, 170);
  queue_worker_ready(POLLOUT);
  queue_ack(input.sequence, 1u);
  mock_supervisor_queue_kill(-1, EPERM);
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_WORKER_REJECTED
  );
  ASSERT(mock_supervisor_kill_call_count() == 1u);
  return true;
}

static bool test_setup_and_fatal_spawn_failures_cleanup(void) {
  const uint8_t payload = 12u;
  const supervised_service_message_t input = message(77u, &payload, 1u);

  mock_supervisor_reset();
  mock_supervisor_fail_pipe2(EMFILE);
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_close_call_count() == 0u);

  mock_supervisor_reset();
  mock_supervisor_fail_sigaction_call(1u, EINVAL);
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_sigaction_call_count() == 1u);
  ASSERT(mock_supervisor_close_call_count() == 2u);

  mock_supervisor_reset();
  mock_supervisor_fail_sigaction_call(2u, EINVAL);
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_sigaction_call_count() == 3u);
  ASSERT(mock_supervisor_sigaction_is_restore(2u));
  ASSERT(mock_supervisor_sigaction_signal(2u) == SIGINT);
  ASSERT(mock_supervisor_close_call_count() == 2u);

  mock_supervisor_reset();
  mock_supervisor_queue_spawn((mock_supervisor_spawn_step_t){
    .result = -1,
    .error_number = EACCES,
  });
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_spawn_call_count() == 1u);
  ASSERT(mock_supervisor_poll_call_count() == 0u);
  ASSERT(mock_supervisor_sigaction_call_count() == 4u);
  ASSERT(mock_supervisor_close_call_count() == 2u);
  return true;
}

static bool test_unexpected_wake_and_fatal_poll_errors_fail_closed(void) {
  const uint8_t payload = 13u;
  const supervised_service_message_t input = message(88u, &payload, 1u);

  mock_supervisor_reset();
  queue_spawn(9001, 171, 172);
  queue_poll(1, 0, POLLIN, 0, POLLOUT, 0);
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_send_call_count() == 0u);

  mock_supervisor_reset();
  queue_spawn(9002, 173, 174);
  queue_poll(-1, EBADF, 0, 0, 0, 0);
  queue_graceful_cleanup();
  ASSERT(
    supervised_service_run("/worker", &input, 1u) ==
      SUPERVISED_SERVICE_OS_ERROR
  );
  ASSERT(mock_supervisor_send_call_count() == 0u);
  return true;
}

typedef bool (*test_function_t)(void);

typedef struct {
  const char *name;
  test_function_t function;
} test_case_t;

int main(void) {
  const test_case_t tests[] = {
    {
      "invalid arguments have no OS effects",
      test_invalid_arguments_have_no_os_effects,
    },
    {
      "send bounded frames and clean up",
      test_sends_bounded_frames_and_cleans_up,
    },
    {
      "restart and resend unacknowledged message",
      test_restarts_and_resends_unacknowledged_message,
    },
    {
      "restart policy caps consecutive failures",
      test_restart_policy_caps_consecutive_failures,
    },
    {
      "accepted ack resets restart policy",
      test_successful_ack_resets_restart_policy,
    },
    {
      "retryable spawn and channel errors recover",
      test_retryable_spawn_and_channel_errors_recover,
    },
    {
      "channel hangup restarts worker",
      test_channel_hangup_restarts_worker,
    },
    {
      "pidfd exit reaps without signaling dead child",
      test_pidfd_exit_reaps_without_signaling_dead_child,
    },
    {
      "terminal event wins over simultaneous ack readiness",
      test_terminal_event_wins_over_simultaneous_ack_readiness,
    },
    {
      "protocol and worker rejection are terminal",
      test_protocol_and_worker_rejections_are_terminal,
    },
    {
      "short send is fatal",
      test_short_send_is_fatal,
    },
    {
      "stop signal has priority and preserves errno",
      test_stop_signal_has_priority_and_preserves_errno,
    },
    {
      "shutdown escalates after bounded grace",
      test_shutdown_escalates_after_bounded_grace,
    },
    {
      "shutdown failures report OS error and preserve primary",
      test_shutdown_failures_report_os_error_and_preserve_primary,
    },
    {
      "setup and fatal spawn failures clean up",
      test_setup_and_fatal_spawn_failures_cleanup,
    },
    {
      "unexpected wake and fatal poll errors fail closed",
      test_unexpected_wake_and_fatal_poll_errors_fail_closed,
    },
  };
  size_t index;

  for (index = 0u; index < sizeof(tests) / sizeof(tests[0]); ++index) {
    if (!tests[index].function()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}
