#include <string.h>

#include "json.h"

enum {
  S_VALUE,
  S_VALUE_OR_CLOSE,
  S_KEY,
  S_KEY_OR_CLOSE,
  S_COLON,
  S_NEXT,
  S_STRING,
  S_ESCAPE,
  S_UNICODE,
  S_LITERAL,
  S_DONE,
  S_FAILED
};

#define UNICODE_DIGITS 4
#define ASCII_LIMIT 0x80
#define CONTROL_LIMIT 0x20
#define UTF8_LEAD 0xC0
#define REPLACEMENT '?'

void json_init(json_t *json, json_handler_t handler, void *arg)
{
  memset(json, 0, sizeof(*json));
  json->handler = handler;
  json->arg = arg;
}

bool json_key(const json_t *json, uint8_t depth, const char *key)
{ return depth <= json->depth && !strcmp(json->keys[depth], key); }

static bool is_space(uint8_t c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static bool emit(json_t *json, uint8_t event)
{
  if (json->handler(json, event)) return true;
  json->state = S_FAILED;
  return false;
}

static void append(json_t *json, char c)
{
  if (json->in_key) {
    if (json->key_length < JSON_KEY_SIZE - 1) json->keys[json->depth][json->key_length++] = c;
    else json->key_overflow = true;
    return;
  }
  if (json->length < JSON_VALUE_SIZE - 1) json->value[json->length++] = c;
  else json->truncated = true;
}

static uint8_t after_value(json_t *json) { return json->depth ? S_NEXT : S_DONE; }

static bool open_container(json_t *json, uint8_t type)
{
  if (json->depth == JSON_MAX_DEPTH) return false;
  if (!emit(json, type)) return false;
  json->containers[json->depth++] = type;
  json->keys[json->depth][0] = 0;
  json->state = type == JSON_OBJECT ? S_KEY_OR_CLOSE : S_VALUE_OR_CLOSE;
  return true;
}

static bool close_container(json_t *json, uint8_t type)
{
  if (!json->depth || json->containers[json->depth - 1] != type) return false;
  json->depth--;
  if (!emit(json, JSON_END)) return false;
  json->state = after_value(json);
  return true;
}

static bool begin_value(json_t *json, uint8_t c)
{
  json->length = 0;
  json->truncated = false;
  json->in_key = false;
  if (c == '{') return open_container(json, JSON_OBJECT);
  if (c == '[') return open_container(json, JSON_ARRAY);
  if (c == '"') {
    json->state = S_STRING;
    return true;
  }
  if (c == '-' || (c >= '0' && c <= '9') || c == 't' || c == 'f' || c == 'n') {
    json->value[json->length++] = (char)c;
    json->state = S_LITERAL;
    return true;
  }
  return false;
}

static bool is_literal_char(uint8_t c)
{
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '+' || c == '-';
}

static bool number_ok(const char *s, uint16_t n)
{
  uint16_t i = s[0] == '-';
  if (i >= n || s[i] < '0' || s[i] > '9') return false;
  for (; i < n; i++)
    if (!((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-'))
      return false;
  return true;
}

static bool end_literal(json_t *json)
{
  uint8_t event;
  json->value[json->length] = 0;
  if (json->truncated) return false;
  if (!strcmp(json->value, "true")) event = JSON_TRUE;
  else if (!strcmp(json->value, "false")) event = JSON_FALSE;
  else if (!strcmp(json->value, "null")) event = JSON_NULL;
  else if (number_ok(json->value, json->length)) event = JSON_NUMBER;
  else return false;
  json->state = after_value(json);
  return emit(json, event);
}

static bool end_string(json_t *json)
{
  if (json->in_key) {
    json->keys[json->depth][json->key_overflow ? 0 : json->key_length] = 0;
    json->state = S_COLON;
    return true;
  }
  json->value[json->length] = 0;
  json->state = after_value(json);
  return emit(json, JSON_STRING);
}

static int hex_digit(uint8_t c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool string_char(json_t *json, uint8_t c)
{
  if (c == '"') return end_string(json);
  if (c == '\\') {
    json->state = S_ESCAPE;
    return true;
  }
  if (c < CONTROL_LIMIT) return false;
  if (c < ASCII_LIMIT) append(json, (char)c);
  else if (c >= UTF8_LEAD) append(json, REPLACEMENT);
  return true;
}

static bool escape_char(json_t *json, uint8_t c)
{
  json->state = S_STRING;
  switch (c) {
  case '"':
  case '\\':
  case '/': append(json, (char)c); return true;
  case 'b':
  case 'f':
  case 'n':
  case 'r':
  case 't': append(json, ' '); return true;
  case 'u':
    json->state = S_UNICODE;
    json->unicode = 0;
    json->unicode_digits = 0;
    return true;
  }
  return false;
}

static bool unicode_char(json_t *json, uint8_t c)
{
  int d = hex_digit(c);
  if (d < 0) return false;
  json->unicode = (uint16_t)(json->unicode << 4 | d);
  if (++json->unicode_digits < UNICODE_DIGITS) return true;
  json->state = S_STRING;
  if (json->unicode >= ASCII_LIMIT) append(json, REPLACEMENT);
  else append(json, json->unicode >= CONTROL_LIMIT ? (char)json->unicode : ' ');
  return true;
}

static bool step(json_t *json, uint8_t c)
{
  switch (json->state) {
  case S_STRING: return string_char(json, c);
  case S_ESCAPE: return escape_char(json, c);
  case S_UNICODE: return unicode_char(json, c);
  case S_LITERAL:
    if (is_literal_char(c)) {
      append(json, (char)c);
      return true;
    }
    if (!end_literal(json)) return false;
    return step(json, c);
  }
  if (is_space(c)) return true;
  switch (json->state) {
  case S_VALUE_OR_CLOSE:
    if (c == ']') return close_container(json, JSON_ARRAY);
    return begin_value(json, c);
  case S_VALUE: return begin_value(json, c);
  case S_KEY_OR_CLOSE:
    if (c == '}') return close_container(json, JSON_OBJECT);
    if (c != '"') return false;
    break;
  case S_KEY:
    if (c != '"') return false;
    break;
  case S_COLON:
    if (c != ':') return false;
    json->state = S_VALUE;
    return true;
  case S_NEXT:
    if (c == ',') {
      bool object = json->containers[json->depth - 1] == JSON_OBJECT;
      json->state = object ? S_KEY : S_VALUE;
      return true;
    }
    if (c == '}') return close_container(json, JSON_OBJECT);
    if (c == ']') return close_container(json, JSON_ARRAY);
    return false;
  default: return false;
  }
  json->in_key = true;
  json->key_overflow = false;
  json->key_length = 0;
  json->state = S_STRING;
  return true;
}

bool json_feed(json_t *json, const uint8_t *data, size_t length)
{
  for (size_t i = 0; i < length; i++) {
    if (json->state == S_FAILED) return false;
    if (!step(json, data[i])) {
      json->state = S_FAILED;
      return false;
    }
  }
  return json->state != S_FAILED;
}

bool json_finish(json_t *json)
{
  if (json->state == S_LITERAL && !end_literal(json)) json->state = S_FAILED;
  return json->state == S_DONE;
}
