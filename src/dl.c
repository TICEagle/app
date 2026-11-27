#include <string.h>

#include "dl.h"
#include "var.h"

void dl_init(dl_t *dl, uint8_t type, uint32_t raw_size, const uint8_t *sha256, dl_open_t open, void *arg)
{
  memset(dl, 0, sizeof(*dl));
  dl->type = type;
  dl->raw_size = raw_size;
  dl->open = open;
  dl->arg = arg;
  memcpy(dl->expected, sha256, SHA_SIZE);
  tls_sha256_init(&dl->sha);
}

static bool fail(dl_t *dl, uint8_t error)
{
  if (!dl->error) dl->error = error;
  return false;
}

bool dl_feed(dl_t *dl, const uint8_t *bytes, size_t length)
{
  if (dl->error) return false;
  if (length > dl->raw_size - dl->received) return fail(dl, DL_E_LONG);
  tls_sha256_update(&dl->sha, bytes, length);
  while (length && dl->received < 2) {
    dl->prefix[dl->received++] = *bytes++;
    length--;
    if (dl->received < 2) continue;
    if (!var_prefix_ok(dl->type, dl->prefix, dl->raw_size)) return fail(dl, DL_E_PREFIX);
    if (!(dl->data = dl->open(dl))) return fail(dl, DL_E_MEMORY);
  }
  if (!length) return true;
  memcpy(dl->data + dl->received - 2, bytes, length);
  dl->received += length;
  return true;
}

bool dl_finish(dl_t *dl)
{
  uint8_t digest[SHA_SIZE];
  if (dl->error) return false;
  if (dl->received != dl->raw_size) return fail(dl, DL_E_SHORT);
  tls_sha256_digest(&dl->sha, digest);
  if (memcmp(digest, dl->expected, SHA_SIZE)) return fail(dl, DL_E_HASH);
  return true;
}
