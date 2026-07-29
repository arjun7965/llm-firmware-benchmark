#ifndef SUPERVISED_SERVICE_H
#define SUPERVISED_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#define SUPERVISED_SERVICE_MAX_MESSAGES 8u
#define SUPERVISED_SERVICE_PAYLOAD_BYTES 32u
#define SUPERVISED_SERVICE_ACK_TIMEOUT_MS 1000
#define SUPERVISED_SERVICE_SHUTDOWN_GRACE_MS 500
#define SUPERVISED_SERVICE_KILL_TIMEOUT_MS 1000
#define SUPERVISED_SERVICE_RESTART_INITIAL_MS 100
#define SUPERVISED_SERVICE_RESTART_MAX_MS 400
#define SUPERVISED_SERVICE_MAX_RESTARTS 3u

typedef struct {
  uint32_t sequence;
  size_t length;
  uint8_t payload[SUPERVISED_SERVICE_PAYLOAD_BYTES];
} supervised_service_message_t;

typedef enum {
  SUPERVISED_SERVICE_OK = 0,
  SUPERVISED_SERVICE_INVALID_ARGUMENT,
  SUPERVISED_SERVICE_OS_ERROR,
  SUPERVISED_SERVICE_PROTOCOL_ERROR,
  SUPERVISED_SERVICE_WORKER_REJECTED,
  SUPERVISED_SERVICE_RESTART_LIMIT,
} supervised_service_result_t;

supervised_service_result_t supervised_service_run(
  const char *worker_path,
  const supervised_service_message_t *messages,
  size_t message_count
);

#endif
