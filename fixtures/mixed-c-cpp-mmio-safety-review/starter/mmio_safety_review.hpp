#ifndef MMIO_SAFETY_REVIEW_HPP
#define MMIO_SAFETY_REVIEW_HPP

#include <cstdint>

#include "fixture_mmio_safety.h"

struct mmio_review_findings_t {
  bool dangling_owner_reference;
  std::uint32_t dangling_owner_reference_line;
  bool incompatible_pointer_overlay;
  std::uint32_t incompatible_pointer_overlay_line;
  bool unchecked_narrowing;
  std::uint32_t unchecked_narrowing_line;
  bool copyable_mmio_owner;
  std::uint32_t copyable_mmio_owner_line;
  bool early_return_cleanup_leak;
  std::uint32_t early_return_cleanup_leak_line;
  bool layout_or_width_assumption;
  std::uint32_t layout_or_width_assumption_line;
  bool implicit_essential_type_conversion;
  std::uint32_t implicit_essential_type_conversion_line;
};

bool mmio_review_diagnose(mmio_review_findings_t *findings) noexcept;

enum class mmio_transfer_result {
  invalid_argument,
  busy,
  started,
  pending,
  completed,
  hardware_error,
};

class mmio_transfer_t {
 public:
  explicit mmio_transfer_t() noexcept;
  ~mmio_transfer_t() noexcept;

  mmio_transfer_t(const mmio_transfer_t &) = delete;
  mmio_transfer_t &operator=(const mmio_transfer_t &) = delete;
  mmio_transfer_t(mmio_transfer_t &&other) noexcept;
  mmio_transfer_t &operator=(mmio_transfer_t &&other) noexcept;

  mmio_transfer_result start(
    volatile mmio_registers_t *registers,
    std::uint32_t count
  ) noexcept;
  mmio_transfer_result poll() noexcept;
  void cancel() noexcept;
  bool active() const noexcept;

 private:
  volatile mmio_registers_t *registers_;
  std::uint16_t count_;
  bool active_;
};

#endif
