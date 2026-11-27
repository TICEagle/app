#include <string.h>

#include "check.h"
#include "var.h"

static bool valid(uint8_t type, const char *name) { return var_valid(type, (const uint8_t *)name, strlen(name)); }

void test_var(void)
{
  uint8_t l1[] = {0x5D, 0x00}, l7[] = {0x5D, 0x06}, str1[] = {0xAA, 0x00}, str_bad[] = {0xAA, 0x0A},
          mat[] = {0x5C, 0x02};
  uint8_t named[] = {0x5D, 'H', 'I', 'G', 'H'}, a[VAR_NAME_SIZE] = "GDASH", b[VAR_NAME_SIZE] = "GDASH";
  char out[VAR_DISPLAY_SIZE];
  CHECK(valid(VAR_PROGRAM, "GDASH"));
  CHECK(valid(VAR_PROTECTED, "A1B2C3D4"));
  CHECK(!valid(VAR_PROGRAM, "A1B2C3D4E"));
  CHECK(!valid(VAR_PROGRAM, "1ABC"));
  CHECK(!valid(VAR_PROGRAM, "abc"));
  CHECK(valid(VAR_APPVAR, "GraphX"));
  CHECK(!valid(VAR_APPVAR, ""));
  CHECK(!valid(0x08, "A"));
  CHECK(var_valid(VAR_LIST, l1, 2));
  CHECK(!var_valid(VAR_LIST, l7, 2));
  CHECK(var_valid(VAR_LIST, named, sizeof(named)));
  CHECK(var_valid(VAR_STRING, str1, 2));
  CHECK(!var_valid(VAR_STRING, str_bad, 2));
  CHECK(var_valid(VAR_MATRIX, mat, 2));
  CHECK(!var_valid(VAR_MATRIX, mat, 1));
  CHECK(var_archivable(VAR_PROTECTED) && var_archivable(VAR_APPVAR) && var_archivable(VAR_STRING));
  CHECK(var_archivable(VAR_MATRIX) && !var_archivable(VAR_PICTURE) && !var_archivable(VAR_LIST));
  CHECK(!var_archivable(VAR_IMAGE));
  CHECK(var_same(VAR_PROGRAM, a, VAR_PROTECTED, b));
  CHECK(!var_same(VAR_APPVAR, a, VAR_PROTECTED, b));
  uint8_t p_ok[] = {0x10, 0x00}, list3[] = {3, 0}, list0[] = {0, 0}, m22[] = {2, 2}, m100[] = {100, 1},
          big[] = {0xE0, 0xFF};
  CHECK(var_prefix_ok(VAR_PROGRAM, p_ok, 18));
  CHECK(!var_prefix_ok(VAR_PROGRAM, p_ok, 19));
  CHECK(var_prefix_ok(VAR_LIST, list3, 29));
  CHECK(!var_prefix_ok(VAR_LIST, list0, 2));
  CHECK(var_prefix_ok(VAR_MATRIX, m22, 38));
  CHECK(!var_prefix_ok(VAR_MATRIX, m100, 902));
  CHECK(var_prefix_ok(VAR_APPVAR, big, VAR_RAW_MAX));
  uint8_t over[] = {0xE1, 0xFF};
  CHECK(!var_prefix_ok(VAR_APPVAR, over, VAR_RAW_MAX + 1));
  var_display(out, VAR_PROGRAM, a);
  CHECK(!strcmp(out, "GDASH"));
  uint8_t l1n[VAR_NAME_SIZE] = {0x5D, 0x00}, nn[VAR_NAME_SIZE] = {0x5D, 'H', 'I'}, s0[VAR_NAME_SIZE] = {0xAA, 0x09};
  var_display(out, VAR_LIST, l1n);
  CHECK(!strcmp(out, "L1"));
  var_display(out, VAR_LIST, nn);
  CHECK(!strcmp(out, "LHI"));
  var_display(out, VAR_STRING, s0);
  CHECK(!strcmp(out, "Str0"));
  uint8_t theta[VAR_NAME_SIZE] = {'A', 0x5B, 'B', 'C', 'D', 'E', 'F', 'G'};
  var_display(out, VAR_PROGRAM, theta);
  CHECK(!strcmp(out, "A@BCDEFG"));
}
