#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "linker_memory_map.h"
#include "mock_linker_memory_map.h"

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      fprintf(stderr, "%s:%d: check failed: %s\n", \
        __FILE__, __LINE__, #condition); \
      return false; \
    } \
  } while (false)

#define IMAGE_END (LINKER_MEMORY_FLASH_BASE + UINT32_C(0x0200))
#define DATA_LOAD_START (LINKER_MEMORY_FLASH_BASE + UINT32_C(0x0180))
#define DATA_LENGTH UINT32_C(8)
#define DATA_LOAD_END (DATA_LOAD_START + DATA_LENGTH)
#define DATA_RAM_START (LINKER_MEMORY_SRAM_BASE + UINT32_C(0x0020))
#define DATA_RAM_END (DATA_RAM_START + DATA_LENGTH)
#define BSS_RAM_START DATA_RAM_END
#define BSS_LENGTH UINT32_C(12)
#define BSS_RAM_END (BSS_RAM_START + BSS_LENGTH)
#define STACK_LIMIT (LINKER_MEMORY_SRAM_BASE + UINT32_C(0x0100))
#define STACK_TOP (LINKER_MEMORY_SRAM_BASE + UINT32_C(0x0180))
#define UNRELATED_SRAM_ADDRESS (LINKER_MEMORY_SRAM_BASE + UINT32_C(0x0200))

static const uint32_t valid_symbols[LINKER_MEMORY_SYMBOL_COUNT] = {
  [LINKER_MEMORY_SYMBOL_IMAGE_START] = LINKER_MEMORY_FLASH_BASE,
  [LINKER_MEMORY_SYMBOL_IMAGE_END] = IMAGE_END,
  [LINKER_MEMORY_SYMBOL_DATA_LOAD_START] = DATA_LOAD_START,
  [LINKER_MEMORY_SYMBOL_DATA_LOAD_END] = DATA_LOAD_END,
  [LINKER_MEMORY_SYMBOL_DATA_RAM_START] = DATA_RAM_START,
  [LINKER_MEMORY_SYMBOL_DATA_RAM_END] = DATA_RAM_END,
  [LINKER_MEMORY_SYMBOL_BSS_RAM_START] = BSS_RAM_START,
  [LINKER_MEMORY_SYMBOL_BSS_RAM_END] = BSS_RAM_END,
  [LINKER_MEMORY_SYMBOL_STACK_LIMIT] = STACK_LIMIT,
  [LINKER_MEMORY_SYMBOL_STACK_TOP] = STACK_TOP,
};

static uint8_t data_value(uint32_t offset) {
  return (uint8_t)(UINT8_C(0x30) + offset);
}

static bool state_equals(
  const linker_memory_map_t *left,
  const linker_memory_map_t *right
) {
  return left->layout == right->layout &&
    left->image_start == right->image_start &&
    left->image_end == right->image_end &&
    left->data_ram_start == right->data_ram_start &&
    left->data_ram_end == right->data_ram_end &&
    left->bss_ram_start == right->bss_ram_start &&
    left->bss_ram_end == right->bss_ram_end &&
    left->stack_limit == right->stack_limit &&
    left->stack_top == right->stack_top &&
    left->initialized == right->initialized;
}

static void copy_valid_symbols(uint32_t *symbols) {
  for (size_t index = 0u;
    index < (size_t)LINKER_MEMORY_SYMBOL_COUNT;
    index++) {
    symbols[index] = valid_symbols[index];
  }
}

static void configure_symbols(const uint32_t *symbols) {
  for (size_t index = 0u;
    index < (size_t)LINKER_MEMORY_SYMBOL_COUNT;
    index++) {
    mock_linker_memory_set_symbol(
      (linker_memory_symbol_t)index,
      symbols[index]
    );
  }
}

static void prepare_memory(void) {
  for (uint32_t offset = 0u; offset < DATA_LENGTH; offset++) {
    mock_linker_memory_set_flash_byte(
      DATA_LOAD_START + offset,
      data_value(offset)
    );
  }
  for (uint32_t address = DATA_RAM_START;
    address < BSS_RAM_END;
    address++) {
    mock_linker_memory_set_sram_byte(address, UINT8_C(0xa5));
  }
  mock_linker_memory_set_sram_byte(UNRELATED_SRAM_ADDRESS, UINT8_C(0x7e));
}

static bool event_matches(
  size_t index,
  mock_linker_memory_event_t event,
  uint32_t address,
  uint32_t value
) {
  return mock_linker_memory_event_at(index) == event &&
    mock_linker_memory_event_address(index) == address &&
    mock_linker_memory_event_value(index) == value;
}

static bool symbol_reads_match(const uint32_t *symbols) {
  if (mock_linker_memory_event_count() <
    (size_t)LINKER_MEMORY_SYMBOL_COUNT) {
    return false;
  }
  for (size_t index = 0u;
    index < (size_t)LINKER_MEMORY_SYMBOL_COUNT;
    index++) {
    if (!event_matches(
      index,
      MOCK_LINKER_MEMORY_EVENT_SYMBOL_READ,
      (uint32_t)index,
      symbols[index]
    )) {
      return false;
    }
  }
  return true;
}

static bool transfers_match(void) {
  size_t index = (size_t)LINKER_MEMORY_SYMBOL_COUNT;

  for (uint32_t offset = 0u; offset < DATA_LENGTH; offset++) {
    if (!event_matches(
      index++,
      MOCK_LINKER_MEMORY_EVENT_FLASH_READ,
      DATA_LOAD_START + offset,
      data_value(offset)
    ) || !event_matches(
      index++,
      MOCK_LINKER_MEMORY_EVENT_SRAM_WRITE,
      DATA_RAM_START + offset,
      data_value(offset)
    )) {
      return false;
    }
  }
  for (uint32_t offset = 0u; offset < BSS_LENGTH; offset++) {
    if (!event_matches(
      index++,
      MOCK_LINKER_MEMORY_EVENT_SRAM_WRITE,
      BSS_RAM_START + offset,
      0u
    )) {
      return false;
    }
  }
  return index == mock_linker_memory_event_count();
}

static bool initialized_memory_matches(void) {
  for (uint32_t offset = 0u; offset < DATA_LENGTH; offset++) {
    if (mock_linker_memory_sram_byte(DATA_RAM_START + offset) !=
      data_value(offset)) {
      return false;
    }
  }
  for (uint32_t offset = 0u; offset < BSS_LENGTH; offset++) {
    if (mock_linker_memory_sram_byte(BSS_RAM_START + offset) != 0u) {
      return false;
    }
  }
  return mock_linker_memory_sram_byte(UNRELATED_SRAM_ADDRESS) ==
    UINT8_C(0x7e);
}

static bool uninitialized_memory_matches(void) {
  for (uint32_t address = DATA_RAM_START;
    address < BSS_RAM_END;
    address++) {
    if (mock_linker_memory_sram_byte(address) != UINT8_C(0xa5)) {
      return false;
    }
  }
  return mock_linker_memory_sram_byte(UNRELATED_SRAM_ADDRESS) ==
    UINT8_C(0x7e);
}

static bool initialize(linker_memory_map_t *map) {
  return linker_memory_map_initialize(map, mock_linker_memory_layout());
}

static bool invalid_case_is_isolated(
  const uint32_t *symbols,
  const linker_memory_map_t *before
) {
  linker_memory_map_t map = *before;

  mock_linker_memory_reset();
  configure_symbols(symbols);
  prepare_memory();
  CHECK(!initialize(&map));
  CHECK(state_equals(&map, before));
  CHECK(mock_linker_memory_event_count() ==
    (size_t)LINKER_MEMORY_SYMBOL_COUNT);
  CHECK(symbol_reads_match(symbols));
  CHECK(uninitialized_memory_matches());
  CHECK(!mock_linker_memory_invalid_access());
  return true;
}

static bool test_valid_layout_copies_data_and_zeros_bss(void) {
  linker_memory_map_t map = {
    .layout = (volatile linker_memory_layout_t *)(uintptr_t)UINT32_C(1),
    .image_start = UINT32_C(1),
    .image_end = UINT32_C(2),
    .data_ram_start = UINT32_C(3),
    .data_ram_end = UINT32_C(4),
    .bss_ram_start = UINT32_C(5),
    .bss_ram_end = UINT32_C(6),
    .stack_limit = UINT32_C(7),
    .stack_top = UINT32_C(8),
    .initialized = true,
  };

  mock_linker_memory_reset();
  configure_symbols(valid_symbols);
  prepare_memory();

  CHECK(initialize(&map));
  CHECK(mock_linker_memory_event_count() ==
    (size_t)LINKER_MEMORY_SYMBOL_COUNT + (size_t)(DATA_LENGTH * 2u) +
      (size_t)BSS_LENGTH);
  CHECK(symbol_reads_match(valid_symbols));
  CHECK(transfers_match());
  CHECK(map.layout == mock_linker_memory_layout());
  CHECK(map.image_start == LINKER_MEMORY_FLASH_BASE);
  CHECK(map.image_end == IMAGE_END);
  CHECK(map.data_ram_start == DATA_RAM_START);
  CHECK(map.data_ram_end == DATA_RAM_END);
  CHECK(map.bss_ram_start == BSS_RAM_START);
  CHECK(map.bss_ram_end == BSS_RAM_END);
  CHECK(map.stack_limit == STACK_LIMIT);
  CHECK(map.stack_top == STACK_TOP);
  CHECK(map.initialized);
  CHECK(initialized_memory_matches());
  CHECK(!mock_linker_memory_invalid_access());
  return true;
}

static bool test_invalid_layouts_preserve_state_and_memory(void) {
  const linker_memory_map_t before = {
    .layout = (volatile linker_memory_layout_t *)(uintptr_t)UINT32_C(1),
    .image_start = UINT32_C(2),
    .image_end = UINT32_C(3),
    .data_ram_start = UINT32_C(4),
    .data_ram_end = UINT32_C(5),
    .bss_ram_start = UINT32_C(6),
    .bss_ram_end = UINT32_C(7),
    .stack_limit = UINT32_C(8),
    .stack_top = UINT32_C(9),
    .initialized = true,
  };
  uint32_t symbols[LINKER_MEMORY_SYMBOL_COUNT];

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_IMAGE_START] += UINT32_C(4);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_IMAGE_END] = LINKER_MEMORY_FLASH_BASE;
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_START] = LINKER_MEMORY_FLASH_BASE;
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END] = LINKER_MEMORY_FLASH_BASE;
  symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_END] = DATA_RAM_START;
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_START] = DATA_RAM_START;
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_END] = DATA_RAM_START;
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_IMAGE_END] =
    LINKER_MEMORY_FLASH_END + UINT32_C(4);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_IMAGE_END] += UINT32_C(2);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_START] = IMAGE_END;
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END] = IMAGE_END + DATA_LENGTH;
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_START] += UINT32_C(2);
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END] += UINT32_C(2);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END] += UINT32_C(4);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_START] += UINT32_C(2);
  symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_END] += UINT32_C(2);
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_START] += UINT32_C(2);
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_END] += UINT32_C(2);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_START] += UINT32_C(4);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_END] = STACK_LIMIT + UINT32_C(4);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_STACK_LIMIT] += UINT32_C(4);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_STACK_TOP] =
    LINKER_MEMORY_SRAM_END + UINT32_C(8);
  CHECK(invalid_case_is_isolated(symbols, &before));

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_STACK_TOP] = STACK_LIMIT;
  CHECK(invalid_case_is_isolated(symbols, &before));
  return true;
}

static bool test_zero_sections_and_reinitialization_replace_map(void) {
  linker_memory_map_t map = { 0 };
  uint32_t symbols[LINKER_MEMORY_SYMBOL_COUNT];

  copy_valid_symbols(symbols);
  symbols[LINKER_MEMORY_SYMBOL_DATA_LOAD_END] = DATA_LOAD_START;
  symbols[LINKER_MEMORY_SYMBOL_DATA_RAM_END] = DATA_RAM_START;
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_START] = DATA_RAM_START;
  symbols[LINKER_MEMORY_SYMBOL_BSS_RAM_END] = DATA_RAM_START;

  mock_linker_memory_reset();
  configure_symbols(symbols);
  CHECK(initialize(&map));
  CHECK(mock_linker_memory_event_count() ==
    (size_t)LINKER_MEMORY_SYMBOL_COUNT);
  CHECK(symbol_reads_match(symbols));
  CHECK(map.data_ram_start == DATA_RAM_START);
  CHECK(map.data_ram_end == DATA_RAM_START);
  CHECK(map.bss_ram_start == DATA_RAM_START);
  CHECK(map.bss_ram_end == DATA_RAM_START);
  CHECK(map.initialized);

  configure_symbols(valid_symbols);
  prepare_memory();
  mock_linker_memory_clear_log();
  CHECK(initialize(&map));
  CHECK(symbol_reads_match(valid_symbols));
  CHECK(transfers_match());
  CHECK(map.data_ram_end == DATA_RAM_END);
  CHECK(map.bss_ram_end == BSS_RAM_END);
  CHECK(initialized_memory_matches());
  CHECK(!mock_linker_memory_invalid_access());
  return true;
}

static bool test_memory_boundary_queries_are_half_open(void) {
  linker_memory_map_t map = { 0 };
  linker_memory_map_t uninitialized = { 0 };

  mock_linker_memory_reset();
  configure_symbols(valid_symbols);
  prepare_memory();
  CHECK(initialize(&map));
  mock_linker_memory_clear_log();

  CHECK(linker_memory_map_is_image_address(&map, LINKER_MEMORY_FLASH_BASE));
  CHECK(linker_memory_map_is_image_address(&map, IMAGE_END - UINT32_C(1)));
  CHECK(!linker_memory_map_is_image_address(&map, IMAGE_END));
  CHECK(!linker_memory_map_is_image_address(&map, LINKER_MEMORY_SRAM_BASE));

  CHECK(linker_memory_map_is_writable_address(&map, DATA_RAM_START));
  CHECK(linker_memory_map_is_writable_address(
    &map,
    BSS_RAM_END - UINT32_C(1)
  ));
  CHECK(!linker_memory_map_is_writable_address(&map, BSS_RAM_END));
  CHECK(!linker_memory_map_is_writable_address(&map, STACK_LIMIT));

  CHECK(linker_memory_map_is_stack_address(&map, STACK_LIMIT));
  CHECK(linker_memory_map_is_stack_address(
    &map,
    STACK_TOP - UINT32_C(1)
  ));
  CHECK(!linker_memory_map_is_stack_address(&map, STACK_TOP));
  CHECK(!linker_memory_map_is_stack_address(&map, BSS_RAM_END));

  CHECK(!linker_memory_map_is_image_address(NULL, LINKER_MEMORY_FLASH_BASE));
  CHECK(!linker_memory_map_is_writable_address(
    &uninitialized,
    DATA_RAM_START
  ));
  CHECK(!linker_memory_map_is_stack_address(&uninitialized, STACK_LIMIT));
  CHECK(mock_linker_memory_event_count() == 0u);
  CHECK(!mock_linker_memory_invalid_access());
  return true;
}

static bool test_invalid_arguments_have_no_side_effects(void) {
  linker_memory_map_t map = {
    .layout = (volatile linker_memory_layout_t *)(uintptr_t)UINT32_C(1),
    .image_start = UINT32_C(2),
    .image_end = UINT32_C(3),
    .data_ram_start = UINT32_C(4),
    .data_ram_end = UINT32_C(5),
    .bss_ram_start = UINT32_C(6),
    .bss_ram_end = UINT32_C(7),
    .stack_limit = UINT32_C(8),
    .stack_top = UINT32_C(9),
    .initialized = true,
  };
  const linker_memory_map_t before = map;

  mock_linker_memory_reset();
  CHECK(!linker_memory_map_initialize(NULL, mock_linker_memory_layout()));
  CHECK(!linker_memory_map_initialize(&map, NULL));
  CHECK(state_equals(&map, &before));
  CHECK(mock_linker_memory_event_count() == 0u);
  CHECK(!mock_linker_memory_invalid_access());
  return true;
}

int main(void) {
  const struct {
    const char *name;
    bool (*run)(void);
  } tests[] = {
    { "valid layout copies data and clears bss", test_valid_layout_copies_data_and_zeros_bss },
    { "invalid layouts preserve state and memory", test_invalid_layouts_preserve_state_and_memory },
    { "zero sections and reinitialization", test_zero_sections_and_reinitialization_replace_map },
    { "half-open memory boundaries", test_memory_boundary_queries_are_half_open },
    { "invalid argument isolation", test_invalid_arguments_have_no_side_effects },
  };

  for (size_t index = 0u; index < sizeof(tests) / sizeof(tests[0]); index++) {
    if (!tests[index].run()) {
      fprintf(stderr, "failed: %s\n", tests[index].name);
      return 1;
    }
  }

  printf("Linker-memory-map public tests passed (%zu tests).\n",
    sizeof(tests) / sizeof(tests[0]));
  return 0;
}
