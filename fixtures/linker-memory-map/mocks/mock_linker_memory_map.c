#include "mock_linker_memory_map.h"

#define MOCK_LINKER_MEMORY_EVENT_CAPACITY 1024u

struct linker_memory_layout {
  uint32_t reserved;
};

typedef struct {
  mock_linker_memory_event_t event;
  uint32_t address;
  uint32_t value;
} mock_linker_memory_event_record_t;

typedef struct {
  struct linker_memory_layout layout;
  uint32_t symbols[LINKER_MEMORY_SYMBOL_COUNT];
  uint8_t flash[LINKER_MEMORY_FLASH_SIZE];
  uint8_t sram[LINKER_MEMORY_SRAM_SIZE];
  mock_linker_memory_event_record_t events[MOCK_LINKER_MEMORY_EVENT_CAPACITY];
  size_t event_count;
  bool invalid_access;
} mock_linker_memory_state_t;

static mock_linker_memory_state_t state;

static bool is_layout(const volatile linker_memory_layout_t *layout) {
  return layout == &state.layout;
}

static bool address_is_in_region(
  uint32_t address,
  uint32_t region_start,
  uint32_t region_end
) {
  return address >= region_start && address < region_end;
}

static void record_event(
  mock_linker_memory_event_t event,
  uint32_t address,
  uint32_t value
) {
  if (state.event_count >= MOCK_LINKER_MEMORY_EVENT_CAPACITY) {
    state.invalid_access = true;
    return;
  }
  state.events[state.event_count++] = (mock_linker_memory_event_record_t) {
    .event = event,
    .address = address,
    .value = value,
  };
}

void mock_linker_memory_reset(void) {
  state = (mock_linker_memory_state_t) { 0 };
}

volatile linker_memory_layout_t *mock_linker_memory_layout(void) {
  return &state.layout;
}

void mock_linker_memory_set_symbol(
  linker_memory_symbol_t symbol,
  uint32_t address
) {
  if ((uint32_t)symbol >= (uint32_t)LINKER_MEMORY_SYMBOL_COUNT) {
    state.invalid_access = true;
    return;
  }
  state.symbols[(size_t)symbol] = address;
}

void mock_linker_memory_set_flash_byte(uint32_t address, uint8_t value) {
  if (!address_is_in_region(
    address,
    LINKER_MEMORY_FLASH_BASE,
    LINKER_MEMORY_FLASH_END
  )) {
    state.invalid_access = true;
    return;
  }
  state.flash[address - LINKER_MEMORY_FLASH_BASE] = value;
}

void mock_linker_memory_set_sram_byte(uint32_t address, uint8_t value) {
  if (!address_is_in_region(
    address,
    LINKER_MEMORY_SRAM_BASE,
    LINKER_MEMORY_SRAM_END
  )) {
    state.invalid_access = true;
    return;
  }
  state.sram[address - LINKER_MEMORY_SRAM_BASE] = value;
}

uint8_t mock_linker_memory_flash_byte(uint32_t address) {
  if (!address_is_in_region(
    address,
    LINKER_MEMORY_FLASH_BASE,
    LINKER_MEMORY_FLASH_END
  )) {
    state.invalid_access = true;
    return 0u;
  }
  return state.flash[address - LINKER_MEMORY_FLASH_BASE];
}

uint8_t mock_linker_memory_sram_byte(uint32_t address) {
  if (!address_is_in_region(
    address,
    LINKER_MEMORY_SRAM_BASE,
    LINKER_MEMORY_SRAM_END
  )) {
    state.invalid_access = true;
    return 0u;
  }
  return state.sram[address - LINKER_MEMORY_SRAM_BASE];
}

void mock_linker_memory_clear_log(void) {
  state.event_count = 0u;
}

uint32_t linker_memory_symbol_address(
  const volatile linker_memory_layout_t *layout,
  linker_memory_symbol_t symbol
) {
  if (!is_layout(layout) ||
    (uint32_t)symbol >= (uint32_t)LINKER_MEMORY_SYMBOL_COUNT) {
    state.invalid_access = true;
    return 0u;
  }
  const uint32_t value = state.symbols[(size_t)symbol];
  record_event(
    MOCK_LINKER_MEMORY_EVENT_SYMBOL_READ,
    (uint32_t)symbol,
    value
  );
  return value;
}

uint8_t linker_memory_read_flash_byte(
  const volatile linker_memory_layout_t *layout,
  uint32_t address
) {
  if (!is_layout(layout) || !address_is_in_region(
    address,
    LINKER_MEMORY_FLASH_BASE,
    LINKER_MEMORY_FLASH_END
  )) {
    state.invalid_access = true;
    return 0u;
  }
  const uint8_t value = state.flash[address - LINKER_MEMORY_FLASH_BASE];
  record_event(MOCK_LINKER_MEMORY_EVENT_FLASH_READ, address, value);
  return value;
}

void linker_memory_write_sram_byte(
  volatile linker_memory_layout_t *layout,
  uint32_t address,
  uint8_t value
) {
  if (!is_layout(layout) || !address_is_in_region(
    address,
    LINKER_MEMORY_SRAM_BASE,
    LINKER_MEMORY_SRAM_END
  )) {
    state.invalid_access = true;
    return;
  }
  state.sram[address - LINKER_MEMORY_SRAM_BASE] = value;
  record_event(MOCK_LINKER_MEMORY_EVENT_SRAM_WRITE, address, value);
}

size_t mock_linker_memory_event_count(void) {
  return state.event_count;
}

mock_linker_memory_event_t mock_linker_memory_event_at(size_t index) {
  if (index >= state.event_count) return MOCK_LINKER_MEMORY_EVENT_COUNT;
  return state.events[index].event;
}

uint32_t mock_linker_memory_event_address(size_t index) {
  if (index >= state.event_count) return 0u;
  return state.events[index].address;
}

uint32_t mock_linker_memory_event_value(size_t index) {
  if (index >= state.event_count) return 0u;
  return state.events[index].value;
}

bool mock_linker_memory_invalid_access(void) {
  return state.invalid_access;
}
