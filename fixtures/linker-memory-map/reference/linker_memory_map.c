#include <stddef.h>

#include "linker_memory_map.h"

static bool range_is_within(
  uint32_t start,
  uint32_t end,
  uint32_t region_start,
  uint32_t region_end
) {
  return start <= end && start >= region_start && end <= region_end;
}

static bool is_aligned(uint32_t address, uint32_t alignment) {
  return address % alignment == 0u;
}

static bool layout_symbols_are_valid(const uint32_t *symbols) {
  const uint32_t image_start = symbols[LINKER_MEMORY_SYMBOL_IMAGE_START];
  const uint32_t image_end = symbols[LINKER_MEMORY_SYMBOL_IMAGE_END];
  const uint32_t data_load_start =
    symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_START];
  const uint32_t data_load_end = symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END];
  const uint32_t data_ram_start =
    symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_START];
  const uint32_t data_ram_end =
    symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_END];
  const uint32_t bss_ram_start =
    symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_START];
  const uint32_t bss_ram_end = symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_END];
  const uint32_t stack_limit = symbols[LINKER_MEMORY_SYMBOL_STACK_LIMIT];
  const uint32_t stack_top = symbols[LINKER_MEMORY_SYMBOL_STACK_TOP];

  if (
    image_start != LINKER_MEMORY_FLASH_BASE || image_start == image_end ||
    !range_is_within(
      image_start,
      image_end,
      LINKER_MEMORY_FLASH_BASE,
      LINKER_MEMORY_FLASH_END
    )
  ) {
    return false;
  }
  if (
    !range_is_within(
      data_load_start,
      data_load_end,
      image_start,
      image_end
    ) ||
    !range_is_within(
      data_ram_start,
      data_ram_end,
      LINKER_MEMORY_SRAM_BASE,
      LINKER_MEMORY_SRAM_END
    ) ||
    !range_is_within(
      bss_ram_start,
      bss_ram_end,
      LINKER_MEMORY_SRAM_BASE,
      LINKER_MEMORY_SRAM_END
    ) ||
    !range_is_within(
      stack_limit,
      stack_top,
      LINKER_MEMORY_SRAM_BASE,
      LINKER_MEMORY_SRAM_END
    )
  ) {
    return false;
  }
  if (
    !is_aligned(image_start, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(image_end, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(data_load_start, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(data_load_end, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(data_ram_start, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(data_ram_end, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(bss_ram_start, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(bss_ram_end, LINKER_MEMORY_SECTION_ALIGNMENT) ||
    !is_aligned(stack_limit, LINKER_MEMORY_STACK_ALIGNMENT) ||
    !is_aligned(stack_top, LINKER_MEMORY_STACK_ALIGNMENT)
  ) {
    return false;
  }
  if (
    data_load_end - data_load_start !=
      data_ram_end - data_ram_start ||
    data_ram_end != bss_ram_start || bss_ram_end > stack_limit ||
    stack_limit == stack_top
  ) {
    return false;
  }
  return true;
}

static bool map_is_ready(const linker_memory_map_t *map) {
  return map != NULL && map->initialized && map->layout != NULL;
}

bool linker_memory_map_initialize(
  linker_memory_map_t *map,
  volatile linker_memory_layout_t *layout
) {
  uint32_t symbols[LINKER_MEMORY_SYMBOL_COUNT];

  if (map == NULL || layout == NULL) return false;

  for (size_t index = 0u;
    index < (size_t)LINKER_MEMORY_SYMBOL_COUNT;
    index++) {
    symbols[index] = linker_memory_symbol_address(
      layout,
      (linker_memory_symbol_t)index
    );
  }
  if (!layout_symbols_are_valid(symbols)) return false;

  const uint32_t data_load_start =
    symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_START];
  const uint32_t data_load_end = symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END];
  const uint32_t data_ram_start =
    symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_START];
  const uint32_t data_count = data_load_end - data_load_start;
  const uint32_t bss_ram_start =
    symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_START];
  const uint32_t bss_ram_end = symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_END];
  const uint32_t bss_count = bss_ram_end - bss_ram_start;

  for (uint32_t offset = 0u; offset < data_count; offset++) {
    const uint8_t value = linker_memory_read_flash_byte(
      layout,
      data_load_start + offset
    );
    linker_memory_write_sram_byte(layout, data_ram_start + offset, value);
  }
  for (uint32_t offset = 0u; offset < bss_count; offset++) {
    linker_memory_write_sram_byte(layout, bss_ram_start + offset, 0u);
  }

  *map = (linker_memory_map_t) {
    .layout = layout,
    .image_start = symbols[LINKER_MEMORY_SYMBOL_IMAGE_START],
    .image_end = symbols[LINKER_MEMORY_SYMBOL_IMAGE_END],
    .data_ram_start = data_ram_start,
    .data_ram_end = symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_END],
    .bss_ram_start = bss_ram_start,
    .bss_ram_end = bss_ram_end,
    .stack_limit = symbols[LINKER_MEMORY_SYMBOL_STACK_LIMIT],
    .stack_top = symbols[LINKER_MEMORY_SYMBOL_STACK_TOP],
    .initialized = true,
  };
  return true;
}

bool linker_memory_map_is_image_address(
  const linker_memory_map_t *map,
  uint32_t address
) {
  return map_is_ready(map) && address >= map->image_start &&
    address < map->image_end;
}

bool linker_memory_map_is_writable_address(
  const linker_memory_map_t *map,
  uint32_t address
) {
  return map_is_ready(map) && address >= map->data_ram_start &&
    address < map->bss_ram_end;
}

bool linker_memory_map_is_stack_address(
  const linker_memory_map_t *map,
  uint32_t address
) {
  return map_is_ready(map) && address >= map->stack_limit &&
    address < map->stack_top;
}
