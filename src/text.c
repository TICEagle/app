#include <stdarg.h>

#include "text.h"

#define DIGITS_MAX 10

static size_t put(char *out, size_t size, size_t n, char c)
{
  if (n + 1 < size) out[n] = c;
  return n + 1;
}

size_t text_format(char *out, size_t size, const char *pattern, ...)
{
  va_list args;
  size_t n = 0;
  va_start(args, pattern);
  for (; *pattern; pattern++) {
    if (*pattern != '%') {
      n = put(out, size, n, *pattern);
      continue;
    }
    pattern++;
    if (*pattern == 's') {
      for (const char *s = va_arg(args, const char *); *s; s++) n = put(out, size, n, *s);
      continue;
    }
    unsigned long value;
    if (*pattern == 'l') {
      pattern++;
      value = va_arg(args, unsigned long);
    } else value = va_arg(args, unsigned int);
    char digits[DIGITS_MAX];
    size_t count = 0;
    do {
      digits[count++] = (char)('0' + value % 10);
      value /= 10;
    } while (value);
    while (count) n = put(out, size, n, digits[--count]);
  }
  va_end(args);
  if (size) out[n < size ? n : size - 1] = 0;
  return n;
}
