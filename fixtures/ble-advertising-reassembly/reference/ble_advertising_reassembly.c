#include "ble_advertising_reassembly.h"

static void clearReport(ble_advertising_report_t *report) {
  for (size_t index = 0u; index < BLE_ADV_ADDRESS_BYTES; index++) {
    report->address[index] = 0u;
  }
  report->flags = 0u;
  for (size_t index = 0u; index < BLE_ADV_MAX_LOCAL_NAME_BYTES; index++) {
    report->local_name[index] = 0u;
  }
  report->local_name_length = 0u;
}

static void copyAddress(
  uint8_t destination[BLE_ADV_ADDRESS_BYTES],
  const uint8_t source[BLE_ADV_ADDRESS_BYTES]
) {
  for (size_t index = 0u; index < BLE_ADV_ADDRESS_BYTES; index++) {
    destination[index] = source[index];
  }
}

static bool addressesMatch(
  const uint8_t left[BLE_ADV_ADDRESS_BYTES],
  const uint8_t right[BLE_ADV_ADDRESS_BYTES]
) {
  for (size_t index = 0u; index < BLE_ADV_ADDRESS_BYTES; index++) {
    if (left[index] != right[index]) return false;
  }
  return true;
}

static void finish(
  ble_advertising_receiver_t *receiver,
  ble_adv_result_t result
) {
  receiver->data_length = 0u;
  receiver->next_sequence = 0u;
  receiver->receiving = false;
  receiver->result = result;
}

static void malformed(ble_advertising_receiver_t *receiver) {
  finish(receiver, BLE_ADV_RESULT_MALFORMED);
}

static bool parseAdvertisingData(ble_advertising_receiver_t *receiver) {
  ble_advertising_report_t report;
  bool flags_found = false;
  bool name_found = false;
  size_t index = 0u;

  clearReport(&report);
  copyAddress(report.address, receiver->address);
  while (index < receiver->data_length) {
    const size_t structure_length = (size_t)receiver->data[index];
    index++;
    if (structure_length == 0u) {
      if (index != receiver->data_length) return false;
      break;
    }
    if (structure_length > receiver->data_length - index) return false;

    const uint8_t type = receiver->data[index];
    const size_t value_length = structure_length - 1u;
    const uint8_t *const value = &receiver->data[index + 1u];
    if (type == BLE_ADV_AD_TYPE_FLAGS) {
      if (flags_found || value_length != 1u) return false;
      report.flags = value[0];
      flags_found = true;
    } else if (type == BLE_ADV_AD_TYPE_COMPLETE_LOCAL_NAME) {
      if (
        name_found ||
        value_length == 0u ||
        value_length > BLE_ADV_MAX_LOCAL_NAME_BYTES
      ) {
        return false;
      }
      for (size_t value_index = 0u; value_index < value_length; value_index++) {
        report.local_name[value_index] = value[value_index];
      }
      report.local_name_length = value_length;
      name_found = true;
    }
    index += structure_length;
  }
  if (!flags_found || !name_found) return false;

  receiver->report = report;
  return true;
}

bool ble_advertising_receiver_init(ble_advertising_receiver_t *receiver) {
  if (receiver == NULL) return false;

  *receiver = (ble_advertising_receiver_t){ 0 };
  receiver->initialized = true;
  return true;
}

bool ble_advertising_receiver_accept(
  ble_advertising_receiver_t *receiver,
  const ble_advertising_fragment_t *fragment,
  uint32_t now_ms
) {
  if (
    receiver == NULL ||
    fragment == NULL ||
    !receiver->initialized ||
    receiver->result != BLE_ADV_RESULT_NONE
  ) {
    return false;
  }
  if (
    receiver->receiving &&
    (uint32_t)(now_ms - receiver->last_fragment_at) >=
      BLE_ADV_REASSEMBLY_TIMEOUT_MS
  ) {
    return false;
  }
  if (
    fragment->data_length == 0u ||
    fragment->data_length > BLE_ADV_MAX_FRAGMENT_BYTES
  ) {
    malformed(receiver);
    return true;
  }

  if (!receiver->receiving) {
    if (fragment->sequence != 0u) {
      malformed(receiver);
      return true;
    }
    receiver->data_length = 0u;
    receiver->next_sequence = 0u;
    receiver->receiving = true;
    copyAddress(receiver->address, fragment->address);
  } else if (
    !addressesMatch(receiver->address, fragment->address) ||
    fragment->sequence != receiver->next_sequence
  ) {
    malformed(receiver);
    return true;
  }

  if (
    fragment->data_length >
      BLE_ADV_MAX_DATA_BYTES - receiver->data_length
  ) {
    malformed(receiver);
    return true;
  }
  for (size_t index = 0u; index < fragment->data_length; index++) {
    receiver->data[receiver->data_length + index] = fragment->data[index];
  }
  receiver->data_length += fragment->data_length;
  receiver->last_fragment_at = now_ms;

  if (fragment->more_fragments) {
    receiver->next_sequence = (uint8_t)(fragment->sequence + UINT8_C(1));
    return true;
  }
  if (!parseAdvertisingData(receiver)) {
    malformed(receiver);
    return true;
  }
  finish(receiver, BLE_ADV_RESULT_COMPLETE);
  return true;
}

ble_adv_result_t ble_advertising_receiver_poll(
  ble_advertising_receiver_t *receiver,
  uint32_t now_ms
) {
  if (
    receiver == NULL ||
    !receiver->initialized ||
    !receiver->receiving ||
    receiver->result != BLE_ADV_RESULT_NONE
  ) {
    return BLE_ADV_RESULT_NONE;
  }

  const uint32_t elapsed = (uint32_t)(now_ms - receiver->last_fragment_at);
  if (elapsed < BLE_ADV_REASSEMBLY_TIMEOUT_MS) {
    return BLE_ADV_RESULT_NONE;
  }
  finish(receiver, BLE_ADV_RESULT_TIMEOUT);
  return receiver->result;
}

ble_adv_result_t ble_advertising_receiver_take_report(
  ble_advertising_receiver_t *receiver,
  ble_advertising_report_t *report
) {
  if (report == NULL) return BLE_ADV_RESULT_NONE;
  clearReport(report);
  if (
    receiver == NULL ||
    !receiver->initialized ||
    receiver->result == BLE_ADV_RESULT_NONE
  ) {
    return BLE_ADV_RESULT_NONE;
  }

  const ble_adv_result_t result = receiver->result;
  if (result == BLE_ADV_RESULT_COMPLETE) {
    *report = receiver->report;
  }
  clearReport(&receiver->report);
  receiver->result = BLE_ADV_RESULT_NONE;
  return result;
}
