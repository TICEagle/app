#ifndef CEAGLE_API_H
#define CEAGLE_API_H

#include <stdbool.h>
#include <stdint.h>

#include "db.h"
#include "json.h"

#define LIST_MAX 20
#define LIST_NAME_SIZE 33
#define LIST_SUMMARY_SIZE 40
#define NAME_SIZE 65
#define OWNER_SIZE 25
#define SUMMARY_SIZE 201
#define MESSAGE_SIZE 120
#define FILENAME_MAX_LENGTH 64
#define PLAN_MAX_PROJECTS 64
#define PLAN_MAX_FILES 96
#define URL_PATH_SIZE 160
#define PLAN_POOL_SIZE (PLAN_MAX_FILES * (FILENAME_MAX_LENGTH + 1))

enum { KIND_UNKNOWN, KIND_BASIC, KIND_ASM, KIND_LIBRARY, KIND_APPVAR };

enum {
  CAP_FLASH_WRITE = 1,
  CAP_DELETE_VARS = 2,
  CAP_RAM_RESET = 4,
  CAP_USB = 8,
  CAP_NETWORK = 16,
  CAP_INLINE_ASM = 32,
  CAP_LINK = 64,
  CAP_OTHER = 128
};

enum {
  PLAN_OK,
  PLAN_E_INVALID,
  PLAN_E_TOO_MANY_FILES,
  PLAN_E_TOO_MANY_PROJECTS,
  PLAN_E_TOO_LARGE,
  PLAN_E_VERSION,
  PLAN_E_DUPLICATE,
  PLAN_E_OWNED,
  PLAN_E_EXISTS,
  PLAN_E_DB_FULL
};

enum {
  FILE_DOWNLOAD = 0x01,
  FILE_SKIP = 0x02,
  FILE_ADOPT = 0x04,
  FILE_ASK = 0x08,
  FILE_RECORDED = 0x10,
  FILE_ARCHIVED = 0x20
};

enum { PROJECT_PREEXISTING = 1 };

enum { UPDATE_NONE, UPDATE_LATEST, UPDATE_TAKEN_DOWN, UPDATE_UNKNOWN };

typedef struct {
  char slug[SLUG_MAX + 1];
  char name[LIST_NAME_SIZE];
  char summary[LIST_SUMMARY_SIZE];
  char latest[VERSION_MAX + 1];
  uint8_t kind;
} list_item_t;

typedef struct {
  list_item_t items[LIST_MAX];
  uint8_t count;
  bool more;
} project_list_t;

typedef struct {
  char slug[SLUG_MAX + 1];
  char name[NAME_SIZE];
  char owner[OWNER_SIZE];
  char summary[SUMMARY_SIZE];
  char latest[VERSION_MAX + 1];
  uint8_t kind;
  uint8_t caps;
  bool taken_down;
} detail_t;

typedef struct {
  char slug[SLUG_MAX + 1];
  char version[VERSION_MAX + 1];
  uint8_t db;
  uint8_t flags;
} plan_project_t;

typedef struct {
  uint8_t project;
  uint8_t type;
  uint8_t flags;
  uint8_t name[VAR_NAME_SIZE];
  uint32_t raw_size;
  uint8_t sha256[SHA_SIZE];
  uint16_t filename;
} plan_file_t;

typedef struct {
  char root[SLUG_MAX + 1];
  char version[VERSION_MAX + 1];
  uint8_t caps;
  uint8_t error;
  uint8_t project_count;
  uint8_t file_count;
  uint16_t pool_used;
  uint32_t ram_needed;
  uint32_t archive_needed;
  uint32_t download_bytes;
  uint8_t downloads;
  uint8_t replace_asks;
  char problem[VAR_DISPLAY_SIZE + SLUG_MAX + 2];
  plan_project_t projects[PLAN_MAX_PROJECTS];
  plan_file_t files[PLAN_MAX_FILES];
  char pool[PLAN_POOL_SIZE];
  plan_file_t current;
  char current_slug[SLUG_MAX + 1];
  char current_version[VERSION_MAX + 1];
  char current_url[URL_PATH_SIZE];
  uint8_t current_name_length;
  uint8_t seen;
} plan_t;

typedef struct {
  char latest[VERSION_MAX + 1];
  uint8_t caps;
  uint8_t state;
} update_t;

typedef struct {
  db_t *db;
  update_t items[DB_MAX_PROJECTS];
  char slug[SLUG_MAX + 1];
  update_t current;
} updates_t;

uint8_t cap_bit(const char *name);
uint8_t kind_parse(const char *name);
int version_compare(const char *a, const char *b);
bool hex_decode(const char *hex, uint8_t *out, uint8_t max, uint8_t *length);
bool api_list(json_t *json, uint8_t event);
bool api_detail(json_t *json, uint8_t event);
bool api_install(json_t *json, uint8_t event);
bool api_install_done(plan_t *plan);
bool api_updates(json_t *json, uint8_t event);
bool api_error(json_t *json, uint8_t event);

#endif
