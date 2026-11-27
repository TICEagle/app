#ifndef CEAGLE_JSON_H
#define CEAGLE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JSON_MAX_DEPTH 8
#define JSON_KEY_SIZE 16
#define JSON_VALUE_SIZE 208

enum { JSON_STRING, JSON_NUMBER, JSON_TRUE, JSON_FALSE, JSON_NULL, JSON_OBJECT, JSON_ARRAY, JSON_END };

typedef struct json json_t;
typedef bool (*json_handler_t)(json_t *json, uint8_t event);

struct json {
  json_handler_t handler;
  void *arg;
  uint8_t state;
  uint8_t depth;
  uint8_t unicode_digits;
  uint16_t unicode;
  bool in_key;
  bool key_overflow;
  bool truncated;
  uint8_t key_length;
  uint16_t length;
  uint8_t containers[JSON_MAX_DEPTH];
  char keys[JSON_MAX_DEPTH + 1][JSON_KEY_SIZE];
  char value[JSON_VALUE_SIZE];
};

void json_init(json_t *json, json_handler_t handler, void *arg);
bool json_feed(json_t *json, const uint8_t *data, size_t length);
bool json_finish(json_t *json);
bool json_key(const json_t *json, uint8_t depth, const char *key);

#endif
