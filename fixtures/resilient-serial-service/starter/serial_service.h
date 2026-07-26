#ifndef SERIAL_SERVICE_H
#define SERIAL_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SERIAL_SERVICE_BUFFER_BYTES 64u
#define SERIAL_SERVICE_RECONNECT_INITIAL_MS 100
#define SERIAL_SERVICE_RECONNECT_MAX_MS 1600

typedef enum {
  SERIAL_SERVICE_OK = 0,
  SERIAL_SERVICE_INVALID_ARGUMENT,
  SERIAL_SERVICE_OS_ERROR,
  SERIAL_SERVICE_CONSUMER_ERROR,
} serial_service_result_t;

typedef bool (*serial_service_consumer_t)(
  const uint8_t *data,
  size_t length,
  void *context
);

serial_service_result_t serial_service_run(
  const char *device_path,
  serial_service_consumer_t consumer,
  void *context
);

#endif
