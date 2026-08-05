#include <cstdio>
#include <type_traits>
#include <utility>

#include "mmio_safety_review.hpp"
#include "mock_mmio_safety.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

static_assert(!std::is_copy_constructible<mmio_transfer_t>::value, "owner is move-only");
static_assert(!std::is_copy_assignable<mmio_transfer_t>::value, "owner is move-only");
static_assert(std::is_nothrow_move_constructible<mmio_transfer_t>::value, "move is noexcept");
static_assert(std::is_nothrow_move_assignable<mmio_transfer_t>::value, "move is noexcept");

static bool event_equals(
  volatile mmio_registers_t *registers,
  size_t index,
  enum mock_mmio_event_kind kind,
  uint32_t value
) {
  enum mock_mmio_event_kind observed_kind;
  uint32_t observed_value;
  return mock_mmio_event_at(
    registers,
    index,
    &observed_kind,
    &observed_value
  ) && observed_kind == kind && observed_value == value;
}

static bool test_diagnosis(void) {
  mmio_review_findings_t findings{};
  CHECK(!mmio_review_diagnose(nullptr));
  CHECK(mmio_review_diagnose(&findings));
  CHECK(findings.dangling_owner_reference && findings.dangling_owner_reference_line == 17u);
  CHECK(findings.incompatible_pointer_overlay && findings.incompatible_pointer_overlay_line == 21u);
  CHECK(findings.unchecked_narrowing && findings.unchecked_narrowing_line == 22u);
  CHECK(findings.copyable_mmio_owner && findings.copyable_mmio_owner_line == 9u);
  CHECK(findings.early_return_cleanup_leak && findings.early_return_cleanup_leak_line == 24u);
  CHECK(findings.layout_or_width_assumption && findings.layout_or_width_assumption_line == 25u);
  CHECK(findings.implicit_essential_type_conversion && findings.implicit_essential_type_conversion_line == 23u);
  return true;
}

static bool test_start_bounds_and_order(void) {
  volatile mmio_registers_t *first = mock_mmio_create();
  volatile mmio_registers_t *second = mock_mmio_create();
  mmio_transfer_t transfer;
  mock_mmio_reset(first);
  CHECK(transfer.start(nullptr, 1u) == mmio_transfer_result::invalid_argument);
  CHECK(transfer.start(first, 0u) == mmio_transfer_result::invalid_argument);
  CHECK(transfer.start(first, 4096u) == mmio_transfer_result::invalid_argument);
  CHECK(mock_mmio_event_count(first) == 0u);
  CHECK(transfer.start(first, 1u) == mmio_transfer_result::started);
  CHECK(transfer.active());
  CHECK(mock_mmio_transfer_count(first) == 1u);
  CHECK(mock_mmio_event_count(first) == 3u);
  CHECK(event_equals(first, 0u, MOCK_MMIO_EVENT_CLEAR_STATUS, MMIO_STATUS_TERMINAL));
  CHECK(event_equals(first, 1u, MOCK_MMIO_EVENT_WRITE_COUNT, 1u));
  CHECK(event_equals(first, 2u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_START));
  CHECK(transfer.start(nullptr, 1u) == mmio_transfer_result::invalid_argument);
  CHECK(transfer.start(first, 0u) == mmio_transfer_result::invalid_argument);
  CHECK(transfer.start(first, 4096u) == mmio_transfer_result::invalid_argument);
  CHECK(transfer.active());
  CHECK(mock_mmio_transfer_count(first) == 1u);
  CHECK(mock_mmio_event_count(first) == 3u);
  CHECK(transfer.start(first, 255u) == mmio_transfer_result::busy);
  CHECK(mock_mmio_event_count(first) == 3u);
  CHECK(transfer.start(second, 255u) == mmio_transfer_result::busy);
  CHECK(mock_mmio_event_count(second) == 0u);
  transfer.cancel();

  mock_mmio_reset(first);
  CHECK(transfer.start(first, 255u) == mmio_transfer_result::started);
  CHECK(mock_mmio_transfer_count(first) == 255u);
  transfer.cancel();
  mock_mmio_reset(first);
  CHECK(transfer.start(first, 256u) == mmio_transfer_result::started);
  CHECK(mock_mmio_transfer_count(first) == 256u);
  transfer.cancel();
  mock_mmio_reset(first);
  CHECK(transfer.start(first, 4095u) == mmio_transfer_result::started);
  CHECK(mock_mmio_transfer_count(first) == 4095u);
  transfer.cancel();
  return true;
}

static bool test_poll_terminal_and_cancel(void) {
  volatile mmio_registers_t *first = mock_mmio_create();
  mmio_transfer_t transfer;
  mock_mmio_reset(first);
  CHECK(transfer.poll() == mmio_transfer_result::invalid_argument);
  CHECK(mock_mmio_event_count(first) == 0u);
  CHECK(transfer.start(first, 7u) == mmio_transfer_result::started);
  mock_mmio_set_status(first, 0u);
  CHECK(transfer.poll() == mmio_transfer_result::pending);
  CHECK(transfer.active());
  CHECK(mock_mmio_event_count(first) == 4u);
  CHECK(event_equals(first, 3u, MOCK_MMIO_EVENT_READ_STATUS, 0u));
  mock_mmio_set_status(first, MMIO_STATUS_COMPLETE);
  CHECK(transfer.poll() == mmio_transfer_result::completed);
  CHECK(!transfer.active());
  CHECK(mock_mmio_event_count(first) == 7u);
  CHECK(event_equals(first, 4u, MOCK_MMIO_EVENT_READ_STATUS, MMIO_STATUS_COMPLETE));
  CHECK(event_equals(first, 5u, MOCK_MMIO_EVENT_CLEAR_STATUS, MMIO_STATUS_COMPLETE));
  CHECK(event_equals(first, 6u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));
  CHECK(transfer.poll() == mmio_transfer_result::invalid_argument);
  CHECK(mock_mmio_event_count(first) == 7u);

  mock_mmio_reset(first);
  CHECK(transfer.start(first, 9u) == mmio_transfer_result::started);
  mock_mmio_set_status(first, MMIO_STATUS_COMPLETE | MMIO_STATUS_ERROR);
  CHECK(transfer.poll() == mmio_transfer_result::hardware_error);
  CHECK(event_equals(first, 3u, MOCK_MMIO_EVENT_READ_STATUS, MMIO_STATUS_TERMINAL));
  CHECK(event_equals(first, 4u, MOCK_MMIO_EVENT_CLEAR_STATUS, MMIO_STATUS_TERMINAL));
  CHECK(event_equals(first, 5u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));

  mock_mmio_reset(first);
  CHECK(transfer.start(first, 8u) == mmio_transfer_result::started);
  mock_mmio_set_status(first, MMIO_STATUS_ERROR);
  CHECK(transfer.poll() == mmio_transfer_result::hardware_error);
  CHECK(event_equals(first, 3u, MOCK_MMIO_EVENT_READ_STATUS, MMIO_STATUS_ERROR));
  CHECK(event_equals(first, 4u, MOCK_MMIO_EVENT_CLEAR_STATUS, MMIO_STATUS_ERROR));
  CHECK(event_equals(first, 5u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));

  mock_mmio_reset(first);
  CHECK(transfer.start(first, 10u) == mmio_transfer_result::started);
  mock_mmio_set_status(first, MMIO_STATUS_COMPLETE | 4u);
  CHECK(transfer.poll() == mmio_transfer_result::completed);
  CHECK(mock_mmio_status(first) == 4u);
  CHECK(event_equals(
    first,
    3u,
    MOCK_MMIO_EVENT_READ_STATUS,
    MMIO_STATUS_COMPLETE | 4u
  ));
  CHECK(event_equals(first, 4u, MOCK_MMIO_EVENT_CLEAR_STATUS, MMIO_STATUS_COMPLETE));
  CHECK(event_equals(first, 5u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));

  mock_mmio_reset(first);
  CHECK(transfer.start(first, 3u) == mmio_transfer_result::started);
  transfer.cancel();
  CHECK(!transfer.active());
  CHECK(mock_mmio_event_count(first) == 4u);
  CHECK(event_equals(first, 3u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));
  transfer.cancel();
  CHECK(mock_mmio_event_count(first) == 4u);
  return true;
}

static bool test_move_and_independent_peripherals(void) {
  volatile mmio_registers_t *first = mock_mmio_create();
  volatile mmio_registers_t *second = mock_mmio_create();
  mock_mmio_reset(first);
  mock_mmio_reset(second);
  mmio_transfer_t source;
  CHECK(source.start(first, 11u) == mmio_transfer_result::started);
  mmio_transfer_t moved(std::move(source));
  CHECK(!source.active());
  CHECK(moved.active());
  source.cancel();
  CHECK(mock_mmio_event_count(first) == 3u);
  mmio_transfer_t *self = &moved;
  moved = std::move(*self);
  CHECK(moved.active());
  moved.cancel();
  CHECK(mock_mmio_event_count(first) == 4u);

  mmio_transfer_t destination;
  CHECK(destination.start(second, 12u) == mmio_transfer_result::started);
  CHECK(source.start(first, 13u) == mmio_transfer_result::started);
  destination = std::move(source);
  CHECK(!source.active());
  CHECK(destination.active());
  CHECK(mock_mmio_event_count(second) == 4u);
  CHECK(event_equals(second, 3u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));
  destination.cancel();
  CHECK(mock_mmio_event_count(first) == 8u);
  CHECK(event_equals(first, 7u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));

  mock_mmio_reset(first);
  {
    mmio_transfer_t scoped;
    CHECK(scoped.start(first, 4u) == mmio_transfer_result::started);
  }
  CHECK(mock_mmio_event_count(first) == 4u);
  CHECK(event_equals(first, 3u, MOCK_MMIO_EVENT_WRITE_CONTROL, MMIO_CONTROL_DISABLED));
  return true;
}

int main() {
  const struct {
    const char *name;
    bool (*run)();
  } tests[] = {
    { "diagnosis", test_diagnosis },
    { "start bounds and order", test_start_bounds_and_order },
    { "poll terminal and cancel", test_poll_terminal_and_cancel },
    { "move and independent peripherals", test_move_and_independent_peripherals },
  };
  for (const auto &test : tests) {
    if (!test.run()) return 1;
    std::printf("ok - %s\n", test.name);
  }
  return 0;
}
