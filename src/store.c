#include <fileioc.h>
#include <string.h>
#include <ti/error.h>
#include <ti/vars.h>

#include "plan.h"
#include "store.h"
#include "var.h"

#define DB_APPVAR "CEAGLEDB"
#define SETTINGS_APPVAR "CEAGLECF"
#define SETTINGS_MAGIC 0xCE
#define RAM_RESERVE 2048u
#define LIBRARY_MARKER 0xC0
#define OPTIONAL_LIBRARY_MARKER 0xC1
#define OP1_SIZE 9

settings_t settings;
static bool created;
static void (*gc_before)(void);

void archive_var(const uint8_t *op1);

static void var_name(char *out, const uint8_t *name)
{
  memcpy(out, name, VAR_NAME_SIZE);
  out[VAR_NAME_SIZE] = 0;
}

void store_load_settings(void)
{
  uint8_t h = ti_Open(SETTINGS_APPVAR, "r");
  uint8_t magic = 0;
  strcpy(settings.server, DEFAULT_SERVER);
  settings.prereleases = false;
  if (!h) return;
  if (ti_GetSize(h) == 1 + sizeof(settings) && ti_Read(&magic, 1, 1, h) == 1 && magic == SETTINGS_MAGIC) {
    settings_t loaded;
    if (ti_Read(&loaded, sizeof(loaded), 1, h) == 1 && memchr(loaded.server, 0, SERVER_SIZE) &&
        !strncmp(loaded.server, "http", 4))
      settings = loaded;
  }
  ti_Close(h);
}

static bool write_archived(const char *name, const void *a, size_t an, const void *b, size_t bn, const void *c,
                           size_t cn)
{
  uint8_t h;
  bool ok;
  if (store_free_ram() < an + bn + cn + RAM_RESERVE) return false;
  if (!(h = ti_Open(name, "w"))) return false;
  ok = ti_Write(a, an, 1, h) == 1 && (!bn || ti_Write(b, bn, 1, h) == 1) && (!cn || ti_Write(c, cn, 1, h) == 1);
  if (ok && store_free_archive() > an + bn + cn + PLAN_VAR_OVERHEAD) ti_SetArchiveStatus(true, h);
  ti_Close(h);
  return ok;
}

bool store_save_settings(void)
{
  uint8_t magic = SETTINGS_MAGIC;
  return write_archived(SETTINGS_APPVAR, &magic, 1, &settings, sizeof(settings), NULL, 0);
}

uint8_t store_load_db(db_t *db)
{
  uint8_t h = ti_Open(DB_APPVAR, "r");
  uint8_t result;
  db->project_count = 0;
  db->file_count = 0;
  if (!h) return STORE_DB_EMPTY;
  result = db_parse(db, ti_GetDataPtr(h), ti_GetSize(h)) ? STORE_DB_OK : STORE_DB_DAMAGED;
  ti_Close(h);
  return result;
}

bool store_save_db(const db_t *db)
{
  uint8_t header[DB_HEADER_SIZE];
  db_header(db, header);
  return write_archived(DB_APPVAR, header, DB_HEADER_SIZE, db->projects, db->project_count * sizeof(db_project_t),
                        db->files, db->file_count * sizeof(db_file_t));
}

uint32_t store_free_ram(void) { return os_MemChk(NULL); }

uint32_t store_free_archive(void)
{
  os_ArcChk();
  return os_TempFreeArc;
}

static uint8_t open_var(const uint8_t *name, uint8_t type, const char *mode)
{
  char n[VAR_NAME_SIZE + 1];
  uint8_t h;
  var_name(n, name);
  h = ti_OpenVar(n, mode, type);
  if (!h && type == VAR_PROGRAM) h = ti_OpenVar(n, mode, VAR_PROTECTED);
  if (!h && type == VAR_PROTECTED) h = ti_OpenVar(n, mode, VAR_PROGRAM);
  return h;
}

bool store_exists(uint8_t type, const uint8_t *name)
{
  uint8_t h = open_var(name, type, "r");
  if (h) ti_Close(h);
  return h != 0;
}

void store_delete(uint8_t type, const uint8_t *name)
{
  char n[VAR_NAME_SIZE + 1];
  var_name(n, name);
  ti_DeleteVar(n, type);
  if (type == VAR_PROGRAM || type == VAR_PROTECTED) ti_DeleteVar(n, type == VAR_PROGRAM ? VAR_PROTECTED : VAR_PROGRAM);
}

static bool reserved(uint8_t type, const uint8_t *name)
{
  char n[VAR_NAME_SIZE + 1];
  var_name(n, name);
  return type == VAR_APPVAR && (!strcmp(n, DB_APPVAR) || !strcmp(n, SETTINGS_APPVAR));
}

uint8_t store_probe(uint8_t type, const uint8_t *name, const uint8_t *sha256)
{
  struct tls_sha256_context sha;
  uint8_t digest[SHA_SIZE];
  char n[VAR_NAME_SIZE + 1];
  if (reserved(type, name)) return PROBE_RESERVED;
  uint8_t h = open_var(name, type, "r");
  if (!h) return PROBE_ABSENT;
  const uint8_t *data = ti_GetDataPtr(h);
  uint32_t raw = var_raw_size(type, data - 2);
  bool library = type == VAR_APPVAR && raw >= 4 && data[0] == LIBRARY_MARKER && data[1] == OPTIONAL_LIBRARY_MARKER;
  bool same = false;
  if (raw <= VAR_RAW_MAX) {
    tls_sha256_init(&sha);
    tls_sha256_update(&sha, data - 2, raw);
    tls_sha256_digest(&sha, digest);
    same = !memcmp(digest, sha256, SHA_SIZE);
  }
  ti_Close(h);
  var_name(n, name);
  if (same) return PROBE_SAME;
  if (library) return PROBE_LIBRARY;
  if ((type == VAR_PROGRAM || type == VAR_PROTECTED) && !strcmp(n, APP_NAME)) return PROBE_SELF;
  return PROBE_DIFFERENT;
}

uint8_t *store_open(dl_t *dl)
{
  const plan_file_t *file = dl->arg;
  char n[VAR_NAME_SIZE + 1];
  uint8_t h;
  uint8_t *data;
  uint32_t size = dl->raw_size - 2;
  created = false;
  if (store_free_ram() < dl->raw_size + PLAN_VAR_OVERHEAD + RAM_RESERVE) return NULL;
  store_delete(file->type, file->name);
  var_name(n, file->name);
  if (!(h = ti_OpenVar(n, "w", file->type))) return NULL;
  created = true;
  if (size && (uint32_t)ti_Resize(size, h) != size) {
    ti_Close(h);
    return NULL;
  }
  data = ti_GetDataPtr(h);
  if (file->type == VAR_LIST || file->type == VAR_MATRIX) {
    data[-2] = dl->prefix[0];
    data[-1] = dl->prefix[1];
  }
  ti_Close(h);
  return data;
}

bool store_finish(const plan_file_t *file, bool ok)
{
  uint8_t op1[OP1_SIZE];
  if (!ok) {
    if (created) store_delete(file->type, file->name);
    created = false;
    return false;
  }
  created = false;
  if (!(file->flags & FILE_ARCHIVED)) return true;
  if (store_free_archive() < file->raw_size + PLAN_VAR_OVERHEAD) return true;
  op1[0] = file->type;
  memcpy(op1 + 1, file->name, VAR_NAME_SIZE);
  if (!ti_ArchiveHasRoom(file->raw_size)) gc_before();
  if (!os_PushErrorHandler()) {
    archive_var(op1);
    os_PopErrorHandler();
  }
  return true;
}

void store_gc_hooks(void (*before)(void))
{
  gc_before = before;
  ti_SetGCBehavior(before, NULL);
}
