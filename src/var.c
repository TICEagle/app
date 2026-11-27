#include <string.h>

#include "var.h"

#define THETA 0x5B
#define LIST_TOKEN 0x5D
#define MATRIX_TOKEN 0x5C
#define STRING_TOKEN 0xAA
#define PICTURE_TOKEN 0x60
#define IMAGE_TOKEN 0x3C
#define BUILTIN_LISTS 6
#define NUMBERED_VARS 10
#define LIST_NAME_MAX 5
#define LIST_LENGTH_MAX 999u
#define MATRIX_SIDE_MAX 99u
#define REAL_SIZE 9u

static bool valid_chars(const uint8_t *name, size_t length, size_t max, bool lowercase)
{
  if (!length || length > max) return false;
  for (size_t i = 0; i < length; i++) {
    uint8_t c = name[i];
    if ((c >= 'A' && c <= 'Z') || c == THETA) continue;
    if (lowercase && c >= 'a' && c <= 'z') continue;
    if (i && c >= '0' && c <= '9') continue;
    return false;
  }
  return true;
}

static uint8_t numbered_token(uint8_t type)
{
  switch (type) {
  case VAR_MATRIX: return MATRIX_TOKEN;
  case VAR_STRING: return STRING_TOKEN;
  case VAR_PICTURE: return PICTURE_TOKEN;
  case VAR_IMAGE: return IMAGE_TOKEN;
  }
  return 0;
}

bool var_valid(uint8_t type, const uint8_t *name, size_t length)
{
  uint8_t token;
  switch (type) {
  case VAR_PROGRAM:
  case VAR_PROTECTED: return valid_chars(name, length, VAR_NAME_SIZE, false);
  case VAR_APPVAR: return valid_chars(name, length, VAR_NAME_SIZE, true);
  case VAR_LIST:
    if (length < 2 || name[0] != LIST_TOKEN) return false;
    return (length == 2 && name[1] < BUILTIN_LISTS) || valid_chars(name + 1, length - 1, LIST_NAME_MAX, false);
  }
  token = numbered_token(type);
  return token && length == 2 && name[0] == token && name[1] < NUMBERED_VARS;
}

bool var_archivable(uint8_t type)
{
  return type == VAR_PROGRAM || type == VAR_PROTECTED || type == VAR_APPVAR || type == VAR_STRING || type == VAR_MATRIX;
}

static uint8_t claim_type(uint8_t type) { return type == VAR_PROTECTED ? VAR_PROGRAM : type; }

bool var_same(uint8_t type_a, const uint8_t *name_a, uint8_t type_b, const uint8_t *name_b)
{ return claim_type(type_a) == claim_type(type_b) && !memcmp(name_a, name_b, VAR_NAME_SIZE); }

uint32_t var_raw_size(uint8_t type, const uint8_t *prefix)
{
  uint32_t count = prefix[0] | (uint32_t)prefix[1] << 8;
  if (type == VAR_LIST) return 2 + count * REAL_SIZE;
  if (type == VAR_MATRIX) return 2 + (uint32_t)prefix[0] * prefix[1] * REAL_SIZE;
  return 2 + count;
}

bool var_prefix_ok(uint8_t type, const uint8_t *prefix, uint32_t raw_size)
{
  uint32_t count = prefix[0] | (uint32_t)prefix[1] << 8;
  if (raw_size < 2 || raw_size > VAR_RAW_MAX) return false;
  if (type == VAR_LIST && (count < 1 || count > LIST_LENGTH_MAX)) return false;
  if (type == VAR_MATRIX && (!prefix[0] || !prefix[1] || prefix[0] > MATRIX_SIDE_MAX || prefix[1] > MATRIX_SIDE_MAX))
    return false;
  return var_raw_size(type, prefix) == raw_size;
}

static size_t copy_chars(char *out, const uint8_t *name, size_t max)
{
  size_t n = 0;
  while (n < max && name[n]) {
    out[n] = name[n] == THETA ? '@' : (char)name[n];
    n++;
  }
  return n;
}

void var_display(char *out, uint8_t type, const uint8_t *name)
{
  static const char *const numbered[] = {"Str", "Pic", "Image"};
  size_t n = 0;
  uint8_t digit = (uint8_t)((name[1] + 1) % NUMBERED_VARS);
  switch (type) {
  case VAR_PROGRAM:
  case VAR_PROTECTED:
  case VAR_APPVAR: n = copy_chars(out, name, VAR_NAME_SIZE); break;
  case VAR_LIST:
    out[n++] = 'L';
    if (name[1] < BUILTIN_LISTS) out[n++] = (char)('1' + name[1]);
    else n += copy_chars(out + n, name + 1, LIST_NAME_MAX);
    break;
  case VAR_MATRIX:
    out[n++] = '[';
    out[n++] = (char)('A' + name[1] % NUMBERED_VARS);
    out[n++] = ']';
    break;
  default:
    strcpy(out, numbered[type == VAR_STRING ? 0 : type == VAR_PICTURE ? 1 : 2]);
    n = strlen(out);
    out[n++] = (char)('0' + digit);
  }
  out[n] = 0;
}
