#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ble_advertising_reassembly.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
              __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static void buildFragment(
  ble_advertising_fragment_t *fragment,
  const uint8_t address[BLE_ADV_ADDRESS_BYTES],
  uint8_t sequence,
  bool more_fragments,
  const uint8_t *data,
  uint8_t data_length
) {
  *fragment = (ble_advertising_fragment_t){ 0 };
  for (size_t index = 0u; index < BLE_ADV_ADDRESS_BYTES; index++) {
    fragment->address[index] = address[index];
  }
  fragment->sequence = sequence;
  fragment->more_fragments = more_fragments;
  fragment->data_length = data_length;
  for (size_t index = 0u; index < data_length; index++) {
    fragment->data[index] = data[index];
  }
}

static bool test_multifragment_report_and_result_gate(void) {
  ble_advertising_receiver_t receiver;
  ble_advertising_report_t report = { { UINT8_C(0xFF) }, UINT8_C(0xFF), { 0 }, 99u };
  ble_advertising_fragment_t fragment;
  const uint8_t address[BLE_ADV_ADDRESS_BYTES] = {
    UINT8_C(1), UINT8_C(2), UINT8_C(3),
    UINT8_C(4), UINT8_C(5), UINT8_C(6),
  };
  const uint8_t first[] = {
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x06),
    UINT8_C(7), BLE_ADV_AD_TYPE_COMPLETE_LOCAL_NAME,
    (uint8_t)'B', (uint8_t)'e', (uint8_t)'a',
  };
  const uint8_t second[] = {
    (uint8_t)'c', (uint8_t)'o', (uint8_t)'n',
  };

  CHECK(ble_advertising_receiver_init(&receiver));
  buildFragment(&fragment, address, 0u, true, first, (uint8_t)sizeof(first));
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 10u));
  buildFragment(&fragment, address, 1u, false, second, (uint8_t)sizeof(second));
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 11u));
  CHECK(!ble_advertising_receiver_accept(&receiver, &fragment, 12u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_COMPLETE
  );
  CHECK(memcmp(report.address, address, sizeof(address)) == 0);
  CHECK(report.flags == UINT8_C(0x06));
  CHECK(report.local_name_length == 6u);
  CHECK(memcmp(report.local_name, "Beacon", 6u) == 0);
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_NONE
  );
  CHECK(report.local_name_length == 0u);
  return true;
}

static bool test_source_and_sequence_malformed_recovery(void) {
  ble_advertising_receiver_t receiver;
  ble_advertising_report_t report;
  ble_advertising_fragment_t fragment;
  const uint8_t address_a[BLE_ADV_ADDRESS_BYTES] = {
    UINT8_C(1), UINT8_C(1), UINT8_C(1),
    UINT8_C(1), UINT8_C(1), UINT8_C(1),
  };
  const uint8_t address_b[BLE_ADV_ADDRESS_BYTES] = {
    UINT8_C(2), UINT8_C(2), UINT8_C(2),
    UINT8_C(2), UINT8_C(2), UINT8_C(2),
  };
  const uint8_t flags_only[] = {
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x04),
  };
  const uint8_t valid[] = {
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x06),
    UINT8_C(2), BLE_ADV_AD_TYPE_COMPLETE_LOCAL_NAME, (uint8_t)'A',
  };

  CHECK(ble_advertising_receiver_init(&receiver));
  buildFragment(
    &fragment,
    address_a,
    0u,
    true,
    flags_only,
    (uint8_t)sizeof(flags_only)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 0u));
  buildFragment(
    &fragment,
    address_b,
    1u,
    false,
    valid,
    (uint8_t)sizeof(valid)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 1u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_MALFORMED
  );

  buildFragment(
    &fragment,
    address_a,
    0u,
    true,
    flags_only,
    (uint8_t)sizeof(flags_only)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 2u));
  buildFragment(
    &fragment,
    address_a,
    2u,
    false,
    valid,
    (uint8_t)sizeof(valid)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 3u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_MALFORMED
  );

  buildFragment(
    &fragment,
    address_a,
    0u,
    false,
    valid,
    (uint8_t)sizeof(valid)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 4u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_COMPLETE
  );
  CHECK(report.local_name_length == 1u);
  CHECK(report.local_name[0] == (uint8_t)'A');
  return true;
}

static bool test_malformed_ad_structures(void) {
  ble_advertising_receiver_t receiver;
  ble_advertising_report_t report;
  ble_advertising_fragment_t fragment;
  const uint8_t address[BLE_ADV_ADDRESS_BYTES] = {
    UINT8_C(3), UINT8_C(4), UINT8_C(5),
    UINT8_C(6), UINT8_C(7), UINT8_C(8),
  };
  const uint8_t truncated[] = {
    UINT8_C(3), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x06),
  };
  const uint8_t flags_only[] = {
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x06),
  };
  const uint8_t duplicate_first[] = {
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x06),
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x04),
    UINT8_C(2), BLE_ADV_AD_TYPE_COMPLETE_LOCAL_NAME,
  };
  const uint8_t duplicate_second[] = { (uint8_t)'A' };

  CHECK(ble_advertising_receiver_init(&receiver));
  buildFragment(
    &fragment,
    address,
    0u,
    false,
    truncated,
    (uint8_t)sizeof(truncated)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 0u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_MALFORMED
  );

  buildFragment(
    &fragment,
    address,
    0u,
    false,
    flags_only,
    (uint8_t)sizeof(flags_only)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 1u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_MALFORMED
  );

  buildFragment(
    &fragment,
    address,
    0u,
    true,
    duplicate_first,
    (uint8_t)sizeof(duplicate_first)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 2u));
  buildFragment(
    &fragment,
    address,
    1u,
    false,
    duplicate_second,
    (uint8_t)sizeof(duplicate_second)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 3u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_MALFORMED
  );
  return true;
}

static bool test_capacity_and_wrap_safe_timeout(void) {
  ble_advertising_receiver_t receiver;
  ble_advertising_report_t report;
  ble_advertising_fragment_t fragment;
  const uint8_t address[BLE_ADV_ADDRESS_BYTES] = {
    UINT8_C(9), UINT8_C(8), UINT8_C(7),
    UINT8_C(6), UINT8_C(5), UINT8_C(4),
  };
  const uint8_t eight_bytes[BLE_ADV_MAX_FRAGMENT_BYTES] = { 0 };
  const uint8_t one_byte[] = { UINT8_C(1) };
  const uint8_t flags_only[] = {
    UINT8_C(2), BLE_ADV_AD_TYPE_FLAGS, UINT8_C(0x06),
  };
  const uint32_t started_at = UINT32_MAX - UINT32_C(4);

  CHECK(ble_advertising_receiver_init(&receiver));
  for (uint8_t sequence = 0u; sequence < 3u; sequence++) {
    buildFragment(
      &fragment,
      address,
      sequence,
      true,
      eight_bytes,
      (uint8_t)sizeof(eight_bytes)
    );
    CHECK(
      ble_advertising_receiver_accept(
        &receiver,
        &fragment,
        (uint32_t)sequence
      )
    );
  }
  buildFragment(&fragment, address, 3u, false, one_byte, (uint8_t)sizeof(one_byte));
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, 3u));
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_MALFORMED
  );

  buildFragment(
    &fragment,
    address,
    0u,
    true,
    flags_only,
    (uint8_t)sizeof(flags_only)
  );
  CHECK(ble_advertising_receiver_accept(&receiver, &fragment, started_at));
  CHECK(
    ble_advertising_receiver_poll(&receiver, started_at + UINT32_C(49)) ==
      BLE_ADV_RESULT_NONE
  );
  CHECK(
    ble_advertising_receiver_poll(&receiver, started_at + UINT32_C(50)) ==
      BLE_ADV_RESULT_TIMEOUT
  );
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_TIMEOUT
  );
  CHECK(report.local_name_length == 0u);
  return true;
}

static bool test_null_and_uninitialized_arguments(void) {
  ble_advertising_receiver_t receiver = { 0 };
  ble_advertising_report_t report = { { UINT8_C(0xFF) }, UINT8_C(0xFF), { 0 }, 99u };
  ble_advertising_fragment_t fragment = { 0 };

  CHECK(!ble_advertising_receiver_init(NULL));
  CHECK(!ble_advertising_receiver_accept(&receiver, &fragment, 0u));
  CHECK(ble_advertising_receiver_poll(&receiver, 0u) == BLE_ADV_RESULT_NONE);
  CHECK(
    ble_advertising_receiver_take_report(&receiver, &report) ==
      BLE_ADV_RESULT_NONE
  );
  CHECK(report.flags == 0u);
  CHECK(report.local_name_length == 0u);
  CHECK(ble_advertising_receiver_take_report(&receiver, NULL) == BLE_ADV_RESULT_NONE);
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "multifragment report and result gate", test_multifragment_report_and_result_gate },
    { "source and sequence recovery", test_source_and_sequence_malformed_recovery },
    { "malformed AD structures", test_malformed_ad_structures },
    { "capacity and wrap-safe timeout", test_capacity_and_wrap_safe_timeout },
    { "null and uninitialized arguments", test_null_and_uninitialized_arguments },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) return 1;
    printf("ok - %s\n", tests[index].name);
  }
  return 0;
}
