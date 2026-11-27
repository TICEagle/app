#ifndef CEAGLE_DB_H
#define CEAGLE_DB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "var.h"

#define SLUG_MAX 32
#define VERSION_MAX 24
#define SHA_SIZE 32
#define DB_MAX_PROJECTS 72
#define DB_MAX_FILES 160
#define DB_NEEDS_SIZE ((DB_MAX_PROJECTS + 7) / 8)
#define DB_NONE 0xFF
#define DB_AUTO 1
#define DB_TAKEN_DOWN 2
#define DB_HEADER_SIZE 8

typedef struct {
  char slug[SLUG_MAX + 1];
  char version[VERSION_MAX + 1];
  uint8_t flags;
  uint8_t needs[DB_NEEDS_SIZE];
} db_project_t;

typedef struct {
  uint8_t project;
  uint8_t type;
  uint8_t name[VAR_NAME_SIZE];
  uint8_t sha256[SHA_SIZE];
} db_file_t;

typedef struct {
  uint8_t project_count;
  uint16_t file_count;
  db_project_t projects[DB_MAX_PROJECTS];
  db_file_t files[DB_MAX_FILES];
} db_t;

#define DB_SIZE_MAX (DB_HEADER_SIZE + DB_MAX_PROJECTS * sizeof(db_project_t) + DB_MAX_FILES * sizeof(db_file_t))

bool slug_valid(const char *slug);
bool db_parse(db_t *db, const uint8_t *data, size_t length);
size_t db_header(const db_t *db, uint8_t *out);
uint8_t db_find(const db_t *db, const char *slug);
uint8_t db_add(db_t *db, const char *slug);
void db_remove(db_t *db, uint8_t project);
db_file_t *db_find_file(db_t *db, uint8_t type, const uint8_t *name);
bool db_set_file(db_t *db, uint8_t project, uint8_t type, const uint8_t *name, const uint8_t *sha256);
void db_remove_file(db_t *db, db_file_t *file);
bool db_needs(const db_project_t *project, uint8_t other);
void db_set_need(db_project_t *project, uint8_t other);
uint8_t db_needed_by(const db_t *db, uint8_t project);
uint8_t db_unneeded(const db_t *db);

#endif
