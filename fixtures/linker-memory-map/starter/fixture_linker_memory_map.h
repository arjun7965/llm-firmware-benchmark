#ifndef FIXTURE_LINKER_MEMORY_MAP_H
#define FIXTURE_LINKER_MEMORY_MAP_H

#include <stdint.h>

#define LINKER_MEMORY_FLASH_BASE UINT32_C(0x08000000)
#define LINKER_MEMORY_FLASH_SIZE UINT32_C(0x00004000)
#define LINKER_MEMORY_FLASH_END \
  (LINKER_MEMORY_FLASH_BASE + LINKER_MEMORY_FLASH_SIZE)

#define LINKER_MEMORY_SRAM_BASE UINT32_C(0x20000000)
#define LINKER_MEMORY_SRAM_SIZE UINT32_C(0x00001000)
#define LINKER_MEMORY_SRAM_END \
  (LINKER_MEMORY_SRAM_BASE + LINKER_MEMORY_SRAM_SIZE)

#define LINKER_MEMORY_SECTION_ALIGNMENT UINT32_C(4)
#define LINKER_MEMORY_STACK_ALIGNMENT UINT32_C(8)

typedef struct linker_memory_layout linker_memory_layout_t;

typedef enum {
  LINKER_MEMORY_SYMBOL_IMAGE_START,
  LINKER_MEMORY_SYMBOL_IMAGE_END,
  LINKER_MEMORY_SYMBOL_DATA_LOAD_START,
  LINKER_MEMORY_SYMBOL_DATA_LOAD_END,
  LINKER_MEMORY_SYMBOL_DATA_RAM_START,
  LINKER_MEMORY_SYMBOL_DATA_RAM_END,
  LINKER_MEMORY_SYMBOL_BSS_RAM_START,
  LINKER_MEMORY_SYMBOL_BSS_RAM_END,
  LINKER_MEMORY_SYMBOL_STACK_LIMIT,
  LINKER_MEMORY_SYMBOL_STACK_TOP,
  LINKER_MEMORY_SYMBOL_COUNT,
} linker_memory_symbol_t;

uint32_t linker_memory_symbol_address(
  const volatile linker_memory_layout_t *layout,
  linker_memory_symbol_t symbol
);
uint8_t linker_memory_read_flash_byte(
  const volatile linker_memory_layout_t *layout,
  uint32_t address
);
void linker_memory_write_sram_byte(
  volatile linker_memory_layout_t *layout,
  uint32_t address,
  uint8_t value
);

#endif
