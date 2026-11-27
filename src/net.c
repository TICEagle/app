#include <lwip.h>
#include <string.h>
#include <ti/getcsc.h>

#include "api.h"
#include "http.h"
#include "net.h"
#include "store.h"
#include "ui.h"
#include "text.h"

#define TIMEOUT_MS 30000
#define DHCP_WAIT_MS 30000
#define PROGRESS_STEP 2048
#define HTTP_OK 200
#define READ_SIZE 512
#define REQUEST_SIZE (NET_PATH_SIZE + HTTP_HOST_SIZE + 64)

char net_message[MESSAGE_SIZE];
uint16_t net_status;

static bool started;
static json_t error_json;
static http_t http;
static uint8_t buffer[READ_SIZE];
static char request[REQUEST_SIZE];

static struct {
  net_sink_t sink;
  void *arg;
  uint32_t limit;
  bool refused;
  bool too_long;
} body;

bool net_start(void)
{
  if (!lwip_start() || !lwip_network_up()) return false;
  started = true;
  lwip_request_services(LWIP_SOCKET_SVC_DHCP, 0);
  return true;
}

void net_stop(void)
{
  if (!started) return;
  lwip_stop();
  started = false;
}

void net_idle(void)
{
  if (started) lwip_service_events();
}

static bool deliver(void *arg, const uint8_t *data, size_t length)
{
  (void)arg;
  net_status = http.status;
  if (http.received > body.limit) {
    body.too_long = true;
    return false;
  }
  if (net_status != HTTP_OK) {
    if (http.received == length) json_init(&error_json, api_error, net_message);
    json_feed(&error_json, data, length);
    return true;
  }
  if (body.sink(body.arg, data, length)) return true;
  body.refused = true;
  return false;
}

static uint8_t fail(const char *message)
{
  strcpy(net_message, message);
  return NET_E_CONNECT;
}

static uint8_t exchange(struct lwip_socket *sock, size_t request_length, uint32_t expected)
{
  bool sent = false;
  uint32_t shown = 0;
  for (;;) {
    lwip_service_events();
    if (os_GetCSC() == sk_Clear) return NET_E_CANCEL;
    if (!sent && lwip_socket_status(sock) == LWIP_STATUS_CONNECTED) {
      if (lwip_socket_write(sock, (const uint8_t *)request, request_length) != LWIP_OK)
        return fail("the request could not be sent");
      sent = true;
    }
    size_t n;
    while ((n = lwip_socket_read(sock, buffer, sizeof(buffer)))) {
      if (!http_feed(&http, buffer, n, deliver, NULL)) {
        if (body.too_long || http.too_long) return NET_E_TOO_LONG;
        if (body.refused) return NET_E_DATA;
        return fail("the server's answer is not valid HTTP");
      }
    }
    if (expected && http.received - shown >= PROGRESS_STEP) {
      shown = http.received;
      ui_progress(shown, expected);
    }
    if (lwip_socket_is_active(sock)) continue;
    if (lwip_socket_available(sock)) continue;
    if (lwip_socket_status(sock) != LWIP_STATUS_CLOSED || !sent) return fail("the connection failed");
    if (!http_complete(&http)) return NET_E_SHORT;
    return NET_OK;
  }
}

static bool network_ready(void)
{
  uint32_t start = lwip_now_ms();
  while (!lwip_are_services_ready(NULL, LWIP_SOCKET_SVC_DHCP)) {
    lwip_service_events();
    if (os_GetCSC() == sk_Clear || lwip_now_ms() - start > DHCP_WAIT_MS) return false;
  }
  return true;
}

uint8_t net_get(const char *path, net_sink_t sink, void *arg, uint32_t expected, uint32_t limit)
{
  char host[HTTP_HOST_SIZE];
  uint16_t port;
  size_t length;
  struct lwip_socket *sock;
  memset(&body, 0, sizeof(body));
  body.sink = sink;
  body.arg = arg;
  body.limit = limit;
  net_status = 0;
  net_message[0] = 0;
  http_init(&http);
  if (!started) return fail("the network is not running");
  if (!network_ready()) return fail("no network address yet");
  if (!http_server(settings.server, host, &port)) return fail("the server address is not valid");
  if (!(length = http_request(request, sizeof(request), host, port, path))) return fail("the address is too long");
  if (!(sock = lwip_socket_create(LWIP_SOCKET_TCP, LWIP_NETIF_ANY, NULL, TIMEOUT_MS)))
    return fail("out of network memory");
  uint8_t result = lwip_socket_connect(sock, host, port) == LWIP_OK ? exchange(sock, length, expected)
                                                                    : fail("the connection could not start");
  if (lwip_socket_is_active(sock)) lwip_socket_abort(sock);
  lwip_socket_destroy(sock);
  if (result != NET_OK) return result;
  net_status = http.status;
  if (net_status != HTTP_OK) {
    if (!net_message[0]) text_format(net_message, MESSAGE_SIZE, "The server answered with HTTP %u.", net_status);
    return NET_E_HTTP;
  }
  if (expected) ui_progress(expected, expected);
  return NET_OK;
}

static bool json_sink(void *arg, const uint8_t *data, size_t length) { return json_feed(arg, data, length); }

uint8_t net_get_json(const char *path, json_t *json)
{
  uint8_t result = net_get(path, json_sink, json, 0, NET_JSON_MAX);
  if (result == NET_OK && !json_finish(json)) return NET_E_DATA;
  return result;
}
