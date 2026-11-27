#include <string.h>

#include "check.h"
#include "http.h"
#include "text.h"

static char got[256];
static size_t got_length;
static bool refuse;

static bool sink(void *arg, const uint8_t *data, size_t length)
{
  (void)arg;
  if (got_length + length < sizeof(got)) memcpy(got + got_length, data, length);
  got_length += length;
  return !refuse;
}

static bool run(const char *text, size_t chunk, http_t *http)
{
  size_t n = strlen(text);
  got_length = 0;
  http_init(http);
  for (size_t at = 0; at < n; at += chunk)
    if (!http_feed(http, (const uint8_t *)text + at, at + chunk > n ? n - at : chunk, sink, NULL)) return false;
  return true;
}

static void format(void)
{
  char out[16];
  CHECK(text_format(out, sizeof(out), "a%sb%uc%lu", "xy", 0u, 4294967295ul) == 16 && !strcmp(out, "axyb0c429496729"));
  CHECK(text_format(out, sizeof(out), "%u items", 42u) == 8 && !strcmp(out, "42 items"));
  CHECK(text_format(out, 1, "abc") == 3 && out[0] == 0);
}

void test_http(void)
{
  format();
  http_t http;
  char host[HTTP_HOST_SIZE], req[256];
  uint16_t port;
  CHECK(http_server("http://ceagle.cc", host, &port) && !strcmp(host, "ceagle.cc") && port == 80);
  CHECK(http_server("http://192.168.84.1:8092", host, &port) && !strcmp(host, "192.168.84.1") && port == 8092);
  CHECK(!http_server("https://ceagle.cc", host, &port));
  CHECK(!http_server("http://", host, &port));
  CHECK(!http_server("http://a:", host, &port));
  CHECK(!http_server("http://a:0", host, &port));
  CHECK(!http_server("http://a:65536", host, &port));
  CHECK(!http_server("http://a/b", host, &port));
  CHECK(!http_server("http://a b", host, &port));
  CHECK(!http_server("http://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.cc", host, &port));
  CHECK(http_request(req, sizeof(req), "ceagle.cc", 80, "/api/v1/projects") &&
        !strcmp(req, "GET /api/v1/projects HTTP/1.0\r\nHost: ceagle.cc\r\nUser-Agent: CEagle/1.0.0\r\n\r\n"));
  CHECK(http_request(req, sizeof(req), "h", 8092, "/x") && strstr(req, "Host: h:8092\r\n"));
  CHECK(!http_request(req, 20, "h", 80, "/long/path/that/does/not/fit"));

  static const char ok[] =
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nX-Long: "
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n"
      "content-LENGTH: 5\r\n\r\nhello";
  for (size_t chunk = 1; chunk <= sizeof(ok); chunk++) {
    CHECK(run(ok, chunk, &http) && http_complete(&http));
    CHECK(http.status == 200 && got_length == 5 && !memcmp(got, "hello", 5));
  }
  CHECK(run("HTTP/1.0 404 Not Found\nContent-Length: 2\n\n{}", 3, &http) && http.status == 404 && http_complete(&http));
  CHECK(run("HTTP/1.1 200 OK\r\n\r\nstream until close", 4, &http) && http_complete(&http) && got_length == 18);
  CHECK(run("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nshort", 4, &http) && !http_complete(&http));
  CHECK(!run("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\ntoo long", 4, &http) && http.too_long);
  static const char *const bad[] = {"HTTP/2 200 OK\r\n\r\n",
                                    "HTTP/1.1 20 OK\r\n\r\n",
                                    "HTTP/1.1 999 OK\r\n\r\n",
                                    "HTTP/1.1 abc OK\r\n\r\n",
                                    "SSH-2.0-OpenSSH\r\n\r\n",
                                    "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n",
                                    "HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",
                                    "HTTP/1.1 200 OK\r\nContent-Length: 99999999999\r\n\r\n",
                                    "HTTP/1.1 200 OK\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx",
                                    "HTTP/1.1 200 OK\r\nContent-Length: 1x\r\n\r\nx"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) CHECK(!run(bad[i], 5, &http));
  char huge[HTTP_HEAD_MAX + 100];
  strcpy(huge, "HTTP/1.1 200 OK\r\nX: ");
  memset(huge + strlen(huge), 'a', HTTP_HEAD_MAX);
  strcpy(huge + HTTP_HEAD_MAX, "\r\n\r\n");
  CHECK(!run(huge, 100, &http));
  refuse = true;
  CHECK(!run("HTTP/1.1 200 OK\r\n\r\nx", 50, &http));
  refuse = false;
  for (int round = 0; round < 50000; round++) {
    char buf[sizeof(ok)];
    memcpy(buf, ok, sizeof(ok));
    for (int k = rng() % 3; k >= 0; k--) buf[rng() % (sizeof(ok) - 1)] = (char)rng();
    http_init(&http);
    http_feed(&http, (const uint8_t *)buf, sizeof(ok) - 1, sink, NULL);
  }
}
