#ifndef HOST_HASH_H
#define HOST_HASH_H

#define OPENSSL_API_COMPAT 0x10100000L
#include <openssl/sha.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct tls_sha256_context {
  SHA256_CTX ctx;
};

static inline bool tls_sha256_init(struct tls_sha256_context *c) { return SHA256_Init(&c->ctx) == 1; }
static inline void tls_sha256_update(struct tls_sha256_context *c, const uint8_t *d, size_t n)
{ SHA256_Update(&c->ctx, d, n); }
static inline void tls_sha256_digest(struct tls_sha256_context *c, uint8_t *out) { SHA256_Final(out, &c->ctx); }

#endif
