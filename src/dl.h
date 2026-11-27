#ifndef CEAGLE_DL_H
#define CEAGLE_DL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <lwip/cryptography/hash.h>

#include "db.h"

enum { DL_OK, DL_E_PREFIX, DL_E_LONG, DL_E_SHORT, DL_E_HASH, DL_E_MEMORY };

typedef struct dl dl_t;
typedef uint8_t *(*dl_open_t)(dl_t *dl);

struct dl {
  dl_open_t open;
  void *arg;
  uint8_t type;
  uint8_t error;
  uint32_t raw_size;
  uint32_t received;
  uint8_t prefix[2];
  uint8_t expected[SHA_SIZE];
  uint8_t *data;
  struct tls_sha256_context sha;
};

void dl_init(dl_t *dl, uint8_t type, uint32_t raw_size, const uint8_t *sha256, dl_open_t open, void *arg);
bool dl_feed(dl_t *dl, const uint8_t *bytes, size_t length);
bool dl_finish(dl_t *dl);

#endif
