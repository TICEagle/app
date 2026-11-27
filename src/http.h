#ifndef CEAGLE_HTTP_H
#define CEAGLE_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_LINE_SIZE 64
#define HTTP_HEAD_MAX 8192u
#define HTTP_HOST_SIZE 48
#define HTTP_DEFAULT_PORT 80

typedef bool (*http_sink_t)(void *arg, const uint8_t *data, size_t length);

typedef struct {
  uint8_t state;
  bool has_length;
  bool line_overflow;
  bool too_long;
  uint8_t line_length;
  uint16_t status;
  uint16_t head_size;
  uint32_t length;
  uint32_t received;
  char line[HTTP_LINE_SIZE];
} http_t;

bool http_server(const char *url, char *host, uint16_t *port);
size_t http_request(char *out, size_t size, const char *host, uint16_t port, const char *path);
void http_init(http_t *http);
bool http_feed(http_t *http, const uint8_t *data, size_t length, http_sink_t sink, void *arg);
bool http_complete(const http_t *http);

#endif
