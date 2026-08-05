#include "legacy_mmio.h"

#include <cstdint>

class legacy_owner_t {
 public:
  explicit legacy_owner_t(volatile legacy_mmio_registers_t *registers) : registers_(registers) {}
  ~legacy_owner_t() { registers_->control = 0u; }
  legacy_owner_t(const legacy_owner_t &) = default;

 private:
  volatile legacy_mmio_registers_t *registers_;
};

const legacy_owner_t &escaped_owner(volatile legacy_mmio_registers_t *registers) {
  legacy_owner_t local(registers);
  return local;
}

void configure_legacy(volatile legacy_mmio_registers_t *registers, const std::uint8_t *bytes, std::uint32_t requested_count) {
  volatile std::uint32_t *overlay = reinterpret_cast<volatile std::uint32_t *>(bytes);
  registers->transfer_count = requested_count;
  registers->control = (*overlay & 1u) ? 1u : 0u;
  if (requested_count == 0u) return;
  const long register_address = reinterpret_cast<long>(registers);
  if (register_address == 0L) return;
}
