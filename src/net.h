#ifndef CEAGLE_NET_H
#define CEAGLE_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "json.h"

#define NET_PATH_SIZE 600
#define NET_JSON_MAX (512ul * 1024ul)

enum { NET_OK, NET_E_CONNECT, NET_E_HTTP, NET_E_CANCEL, NET_E_DATA, NET_E_TOO_LONG, NET_E_SHORT };

typedef bool (*net_sink_t)(void *arg, const uint8_t *data, size_t length);

extern char net_message[];
extern uint16_t net_status;

bool net_start(void);
void net_stop(void);
void net_idle(void);
uint8_t net_get(const char *path, net_sink_t sink, void *arg, uint32_t expected, uint32_t limit);
uint8_t net_get_json(const char *path, json_t *json);

#endif
