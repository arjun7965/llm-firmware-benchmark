#ifndef FIRMWARE_STATE_MACHINE_H
#define FIRMWARE_STATE_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_hal.h"

#define TEMPERATURE_SENSOR_ADDRESS UINT8_C(0x48)
#define TEMPERATURE_SENSOR_REGISTER UINT8_C(0x00)
#define TEMPERATURE_SAMPLE_SIZE 2u

typedef struct {
  uint8_t bytes[TEMPERATURE_SAMPLE_SIZE];
  uint32_t timestamp_ms;
  bool valid;
} temperature_sample_t;

void temperature_task_init(void);
void temperature_task_step(void);
bool temperature_task_latest(temperature_sample_t *output);

#endif
