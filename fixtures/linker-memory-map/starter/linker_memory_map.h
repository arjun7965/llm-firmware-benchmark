#ifndef LINKER_MEMORY_MAP_H
#define LINKER_MEMORY_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "fixture_linker_memory_map.h"

typedef struct {
  volatile linker_memory_layout_t *layout;
  uint32_t image_start;
  uint32_t image_end;
  uint32_t data_ram_start;
  uint32_t data_ram_end;
  uint32_t bss_ram_start;
  uint32_t bss_ram_end;
  uint32_t stack_limit;
  uint32_t stack_top;
  bool initialized;
} linker_memory_map_t;

bool linker_memory_map_initialize(
  linker_memory_map_t *map,
  volatile linker_memory_layout_t *layout
);
bool linker_memory_map_is_image_address(
  const linker_memory_map_t *map,
  uint32_t address
);
bool linker_memory_map_is_writable_address(
  const linker_memory_map_t *map,
  uint32_t address
);
bool linker_memory_map_is_stack_address(
  const linker_memory_map_t *map,
  uint32_t address
);

#endif
