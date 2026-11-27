#ifndef CEAGLE_STORE_H
#define CEAGLE_STORE_H

#include <stdbool.h>
#include <stdint.h>

#include "api.h"
#include "db.h"
#include "dl.h"

#define SERVER_SIZE 64
#define DEFAULT_SERVER "http://ceagle.cc"

enum { STORE_DB_EMPTY, STORE_DB_OK, STORE_DB_DAMAGED };

typedef struct {
  char server[SERVER_SIZE];
  bool prereleases;
} settings_t;

extern settings_t settings;

void store_load_settings(void);
bool store_save_settings(void);
uint8_t store_load_db(db_t *db);
bool store_save_db(const db_t *db);
uint32_t store_free_ram(void);
uint32_t store_free_archive(void);
uint8_t store_probe(uint8_t type, const uint8_t *name, const uint8_t *sha256);
uint8_t *store_open(dl_t *dl);
bool store_finish(const plan_file_t *file, bool ok);
bool store_exists(uint8_t type, const uint8_t *name);
void store_delete(uint8_t type, const uint8_t *name);
void store_gc_hooks(void (*before)(void));

#endif
