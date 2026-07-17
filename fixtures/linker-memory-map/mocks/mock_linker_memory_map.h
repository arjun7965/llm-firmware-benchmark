#ifndef MOCK_LINKER_MEMORY_MAP_H
#define MOCK_LINKER_MEMORY_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fixture_linker_memory_map.h"

typedef enum {
  MOCK_LINKER_MEMORY_EVENT_SYMBOL_READ,
  MOCK_LINKER_MEMORY_EVENT_FLASH_READ,
  MOCK_LINKER_MEMORY_EVENT_SRAM_WRITE,
  MOCK_LINKER_MEMORY_EVENT_COUNT,
} mock_linker_memory_event_t;

void mock_linker_memory_reset(void);
volatile linker_memory_layout_t *mock_linker_memory_layout(void);
void mock_linker_memory_set_symbol(
  linker_memory_symbol_t symbol,
  uint32_t address
);
void mock_linker_memory_set_flash_byte(uint32_t address, uint8_t value);
void mock_linker_memory_set_sram_byte(uint32_t address, uint8_t value);
uint8_t mock_linker_memory_flash_byte(uint32_t address);
uint8_t mock_linker_memory_sram_byte(uint32_t address);
void mock_linker_memory_clear_log(void);
size_t mock_linker_memory_event_count(void);
mock_linker_memory_event_t mock_linker_memory_event_at(size_t index);
uint32_t mock_linker_memory_event_address(size_t index);
uint32_t mock_linker_memory_event_value(size_t index);
bool mock_linker_memory_invalid_access(void);

#endif
