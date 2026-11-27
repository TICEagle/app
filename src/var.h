#ifndef CEAGLE_VAR_H
#define CEAGLE_VAR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VAR_NAME_SIZE 8
#define VAR_DATA_MAX 65504u
#define VAR_RAW_MAX (VAR_DATA_MAX + 2u)
#define VAR_DISPLAY_SIZE 12

enum {
  VAR_LIST = 0x01,
  VAR_MATRIX = 0x02,
  VAR_STRING = 0x04,
  VAR_PROGRAM = 0x05,
  VAR_PROTECTED = 0x06,
  VAR_PICTURE = 0x07,
  VAR_APPVAR = 0x15,
  VAR_IMAGE = 0x1A
};

bool var_valid(uint8_t type, const uint8_t *name, size_t length);
bool var_archivable(uint8_t type);
bool var_same(uint8_t type_a, const uint8_t *name_a, uint8_t type_b, const uint8_t *name_b);
bool var_prefix_ok(uint8_t type, const uint8_t *prefix, uint32_t raw_size);
uint32_t var_raw_size(uint8_t type, const uint8_t *prefix);
void var_display(char *out, uint8_t type, const uint8_t *name);

#endif
