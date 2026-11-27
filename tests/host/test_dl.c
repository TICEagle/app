#include <string.h>

#include "check.h"
#include "dl.h"
#include "var.h"

static uint8_t target[VAR_RAW_MAX];
static uint8_t opened;
static bool refuse;

static uint8_t *open_var(dl_t *dl)
{
  (void)dl;
  opened++;
  return refuse ? NULL : target;
}

static void digest(const uint8_t *data, size_t n, uint8_t *out)
{
  struct tls_sha256_context c;
  tls_sha256_init(&c);
  tls_sha256_update(&c, data, n);
  tls_sha256_digest(&c, out);
}

static uint8_t run(uint8_t type, const uint8_t *data, size_t n, uint32_t raw_size, const uint8_t *sha, size_t chunk)
{
  dl_t dl;
  opened = 0;
  dl_init(&dl, type, raw_size, sha, open_var, NULL);
  for (size_t at = 0; at < n; at += chunk)
    if (!dl_feed(&dl, data + at, at + chunk > n ? n - at : chunk)) return dl.error;
  return dl_finish(&dl) ? DL_OK : dl.error;
}

void test_dl(void)
{
  static uint8_t raw[VAR_RAW_MAX];
  uint8_t sha[SHA_SIZE];
  size_t n = VAR_RAW_MAX;
  raw[0] = (uint8_t)(VAR_DATA_MAX & 0xFF);
  raw[1] = (uint8_t)(VAR_DATA_MAX >> 8);
  for (size_t i = 2; i < n; i++) raw[i] = (uint8_t)(i * 7);
  digest(raw, n, sha);
  refuse = false;
  size_t chunks[] = {1, 2, 3, 511, 512, 1460, n};
  for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
    memset(target, 0, sizeof(target));
    CHECK(run(VAR_PROTECTED, raw, n, (uint32_t)n, sha, chunks[i]) == DL_OK);
    CHECK(opened == 1 && !memcmp(target, raw + 2, n - 2));
  }
  CHECK(run(VAR_APPVAR, raw, n - 1, (uint32_t)n, sha, 512) == DL_E_SHORT);
  CHECK(run(VAR_APPVAR, raw, 1, (uint32_t)n, sha, 512) == DL_E_SHORT && opened == 0);
  CHECK(run(VAR_APPVAR, raw, n, (uint32_t)n - 1, sha, 512) == DL_E_PREFIX);
  raw[n - 1] ^= 1;
  CHECK(run(VAR_APPVAR, raw, n, (uint32_t)n, sha, 512) == DL_E_HASH);
  raw[n - 1] ^= 1;
  uint8_t small[20] = {18, 0};
  digest(small, sizeof(small), sha);
  CHECK(run(VAR_PROGRAM, small, sizeof(small), 20, sha, 7) == DL_OK);
  uint8_t longer[30] = {18, 0};
  CHECK(run(VAR_PROGRAM, longer, sizeof(longer), 20, sha, 7) == DL_E_LONG);
  CHECK(run(VAR_PROGRAM, longer, sizeof(longer), 20, sha, 30) == DL_E_LONG);
  refuse = true;
  CHECK(run(VAR_PROGRAM, small, sizeof(small), 20, sha, 7) == DL_E_MEMORY);
  refuse = false;
  uint8_t list[2 + 27] = {3, 0};
  digest(list, sizeof(list), sha);
  CHECK(run(VAR_LIST, list, sizeof(list), sizeof(list), sha, 4) == DL_OK);
  list[0] = 4;
  CHECK(run(VAR_LIST, list, sizeof(list), sizeof(list), sha, 4) == DL_E_PREFIX);
  uint8_t matrix[2 + 4 * 9] = {2, 2};
  digest(matrix, sizeof(matrix), sha);
  CHECK(run(VAR_MATRIX, matrix, sizeof(matrix), sizeof(matrix), sha, 5) == DL_OK);
}
