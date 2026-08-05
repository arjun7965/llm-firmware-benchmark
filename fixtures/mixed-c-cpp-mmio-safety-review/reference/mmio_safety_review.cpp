#include "mmio_safety_review.hpp"

constexpr std::uint32_t maximum_transfer_count = 4095u;

bool mmio_review_diagnose(mmio_review_findings_t *findings) noexcept {
  if (findings == nullptr) return false;
  *findings = {
    true, 17u,
    true, 21u,
    true, 22u,
    true, 9u,
    true, 24u,
    true, 25u,
    true, 23u,
  };
  return true;
}

mmio_transfer_t::mmio_transfer_t() noexcept
  : registers_(nullptr), count_(0u), active_(false) {}

mmio_transfer_t::~mmio_transfer_t() noexcept {
  cancel();
}

mmio_transfer_t::mmio_transfer_t(mmio_transfer_t &&other) noexcept
  : registers_(other.registers_), count_(other.count_), active_(other.active_) {
  other.registers_ = nullptr;
  other.count_ = 0u;
  other.active_ = false;
}

mmio_transfer_t &mmio_transfer_t::operator=(mmio_transfer_t &&other) noexcept {
  if (this != &other) {
    cancel();
    registers_ = other.registers_;
    count_ = other.count_;
    active_ = other.active_;
    other.registers_ = nullptr;
    other.count_ = 0u;
    other.active_ = false;
  }
  return *this;
}

mmio_transfer_result mmio_transfer_t::start(
  volatile mmio_registers_t *registers,
  std::uint32_t count
) noexcept {
  if (registers == nullptr || count == 0u || count > maximum_transfer_count) {
    return mmio_transfer_result::invalid_argument;
  }
  if (active_) return mmio_transfer_result::busy;

  const std::uint16_t checked_count = static_cast<std::uint16_t>(count);
  mmio_write_status_clear(registers, MMIO_STATUS_TERMINAL);
  mmio_write_transfer_count(registers, checked_count);
  mmio_write_control(registers, MMIO_CONTROL_START);
  registers_ = registers;
  count_ = checked_count;
  active_ = true;
  return mmio_transfer_result::started;
}

mmio_transfer_result mmio_transfer_t::poll() noexcept {
  if (!active_) return mmio_transfer_result::invalid_argument;

  const std::uint32_t status = mmio_read_status(registers_);
  const std::uint32_t terminal = status & MMIO_STATUS_TERMINAL;
  if (terminal == 0u) return mmio_transfer_result::pending;

  mmio_write_status_clear(registers_, terminal);
  mmio_write_control(registers_, MMIO_CONTROL_DISABLED);
  const bool is_error = (terminal & MMIO_STATUS_ERROR) != 0u;
  registers_ = nullptr;
  count_ = 0u;
  active_ = false;
  return is_error
    ? mmio_transfer_result::hardware_error
    : mmio_transfer_result::completed;
}

void mmio_transfer_t::cancel() noexcept {
  if (active_) {
    mmio_write_control(registers_, MMIO_CONTROL_DISABLED);
    registers_ = nullptr;
    count_ = 0u;
    active_ = false;
  }
}

bool mmio_transfer_t::active() const noexcept {
  return active_;
}
