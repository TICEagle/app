#include <string.h>

#include "http.h"
#include "text.h"

enum { H_STATUS, H_HEADER, H_BODY, H_FAILED };

#define SCHEME "http://"
#define PORT_MAX 65535u
#define LENGTH_MAX 0xFFFFFFu
#define STATUS_MIN 100
#define STATUS_MAX 599

bool http_server(const char *url, char *host, uint16_t *port)
{
  size_t n;
  const char *colon;
  uint32_t p = 0;
  if (strncmp(url, SCHEME, strlen(SCHEME))) return false;
  url += strlen(SCHEME);
  colon = strchr(url, ':');
  n = colon ? (size_t)(colon - url) : strlen(url);
  if (!n || n >= HTTP_HOST_SIZE) return false;
  for (size_t i = 0; i < n; i++) {
    char c = url[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-'))
      return false;
  }
  memcpy(host, url, n);
  host[n] = 0;
  *port = HTTP_DEFAULT_PORT;
  if (!colon) return true;
  if (!colon[1]) return false;
  for (const char *s = colon + 1; *s; s++) {
    if (*s < '0' || *s > '9') return false;
    p = p * 10 + (uint32_t)(*s - '0');
    if (p > PORT_MAX) return false;
  }
  if (!p) return false;
  *port = (uint16_t)p;
  return true;
}

size_t http_request(char *out, size_t size, const char *host, uint16_t port, const char *path)
{
  size_t n =
      port == HTTP_DEFAULT_PORT
          ? text_format(out, size, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: CEagle/" APP_VERSION "\r\n\r\n", path,
                        host)
          : text_format(out, size, "GET %s HTTP/1.0\r\nHost: %s:%u\r\nUser-Agent: CEagle/" APP_VERSION "\r\n\r\n", path,
                        host, port);
  return n < size ? n : 0;
}

void http_init(http_t *http) { memset(http, 0, sizeof(*http)); }

static char lower(char c) { return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c; }

static bool starts_with(const char *line, const char *prefix)
{
  while (*prefix)
    if (lower(*line++) != *prefix++) return false;
  return true;
}

static bool parse_number(const char *s, uint32_t max, uint32_t *out)
{
  uint32_t v = 0;
  while (*s == ' ' || *s == '\t') s++;
  if (*s < '0' || *s > '9') return false;
  for (; *s >= '0' && *s <= '9'; s++) {
    v = v * 10 + (uint32_t)(*s - '0');
    if (v > max) return false;
  }
  while (*s == ' ' || *s == '\t') s++;
  *out = v;
  return !*s;
}

static bool status_line(http_t *http)
{
  uint32_t status;
  char code[4];
  if (http->line_overflow || strncmp(http->line, "HTTP/1.", 7) || http->line_length < 12 || http->line[8] != ' ')
    return false;
  memcpy(code, http->line + 9, 3);
  code[3] = 0;
  if (!parse_number(code, STATUS_MAX, &status) || status < STATUS_MIN) return false;
  http->status = (uint16_t)status;
  http->state = H_HEADER;
  return true;
}

static bool header_line(http_t *http)
{
  if (!http->line_length) {
    http->state = H_BODY;
    return !http->line_overflow;
  }
  if (starts_with(http->line, "transfer-encoding:")) return false;
  if (!starts_with(http->line, "content-length:")) return true;
  if (http->line_overflow || http->has_length) return false;
  http->has_length = true;
  return parse_number(http->line + 15, LENGTH_MAX, &http->length);
}

static bool end_line(http_t *http)
{
  bool ok;
  if (http->line_length && http->line[http->line_length - 1] == '\r') http->line_length--;
  http->line[http->line_length] = 0;
  ok = http->state == H_STATUS ? status_line(http) : header_line(http);
  http->line_length = 0;
  http->line_overflow = false;
  return ok;
}

bool http_feed(http_t *http, const uint8_t *data, size_t length, http_sink_t sink, void *arg)
{
  while (length && http->state < H_BODY) {
    char c = (char)*data++;
    length--;
    if (++http->head_size > HTTP_HEAD_MAX) goto fail;
    if (c == '\n') {
      if (!end_line(http)) goto fail;
    } else if (http->line_length < HTTP_LINE_SIZE - 1) http->line[http->line_length++] = c;
    else http->line_overflow = true;
  }
  if (http->state == H_FAILED) return false;
  if (!length) return true;
  if (http->has_length && length > http->length - http->received) {
    http->too_long = true;
    goto fail;
  }
  http->received += length;
  if (sink(arg, data, length)) return true;
fail:
  http->state = H_FAILED;
  return false;
}

bool http_complete(const http_t *http)
{ return http->state == H_BODY && (!http->has_length || http->received == http->length); }
