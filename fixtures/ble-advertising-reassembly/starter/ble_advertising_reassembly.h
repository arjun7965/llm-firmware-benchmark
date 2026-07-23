#ifndef BLE_ADVERTISING_REASSEMBLY_H
#define BLE_ADVERTISING_REASSEMBLY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLE_ADV_ADDRESS_BYTES 6u
#define BLE_ADV_MAX_FRAGMENT_BYTES 8u
#define BLE_ADV_MAX_DATA_BYTES 24u
#define BLE_ADV_MAX_LOCAL_NAME_BYTES 8u
#define BLE_ADV_REASSEMBLY_TIMEOUT_MS 50u
#define BLE_ADV_AD_TYPE_FLAGS UINT8_C(0x01)
#define BLE_ADV_AD_TYPE_COMPLETE_LOCAL_NAME UINT8_C(0x09)

typedef enum {
  BLE_ADV_RESULT_NONE = 0,
  BLE_ADV_RESULT_COMPLETE,
  BLE_ADV_RESULT_TIMEOUT,
  BLE_ADV_RESULT_MALFORMED
} ble_adv_result_t;

typedef struct {
  uint8_t address[BLE_ADV_ADDRESS_BYTES];
  uint8_t sequence;
  bool more_fragments;
  uint8_t data_length;
  uint8_t data[BLE_ADV_MAX_FRAGMENT_BYTES];
} ble_advertising_fragment_t;

typedef struct {
  uint8_t address[BLE_ADV_ADDRESS_BYTES];
  uint8_t flags;
  uint8_t local_name[BLE_ADV_MAX_LOCAL_NAME_BYTES];
  size_t local_name_length;
} ble_advertising_report_t;

typedef struct {
  uint8_t data[BLE_ADV_MAX_DATA_BYTES];
  size_t data_length;
  uint8_t address[BLE_ADV_ADDRESS_BYTES];
  uint8_t next_sequence;
  uint32_t last_fragment_at;
  ble_advertising_report_t report;
  ble_adv_result_t result;
  bool initialized;
  bool receiving;
} ble_advertising_receiver_t;

bool ble_advertising_receiver_init(ble_advertising_receiver_t *receiver);
bool ble_advertising_receiver_accept(
  ble_advertising_receiver_t *receiver,
  const ble_advertising_fragment_t *fragment,
  uint32_t now_ms
);
ble_adv_result_t ble_advertising_receiver_poll(
  ble_advertising_receiver_t *receiver,
  uint32_t now_ms
);
ble_adv_result_t ble_advertising_receiver_take_report(
  ble_advertising_receiver_t *receiver,
  ble_advertising_report_t *report
);

#endif
