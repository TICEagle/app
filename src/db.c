#include <string.h>

#include "db.h"

static const uint8_t magic[4] = {'C', 'E', 'D', 'B'};
#define DB_FORMAT 1

bool slug_valid(const char *slug)
{
  size_t n = strlen(slug);
  if (n < 2 || n > SLUG_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = slug[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '-' && i))) return false;
  }
  return true;
}

static bool terminated(const char *s, size_t size) { return memchr(s, 0, size) != NULL; }

static bool file_ok(const db_t *db, const db_file_t *f)
{
  size_t n = VAR_NAME_SIZE;
  while (n && !f->name[n - 1]) n--;
  if (f->type != VAR_PROGRAM && f->type != VAR_PROTECTED && f->type != VAR_APPVAR && n < 2) n = 2;
  return f->project < db->project_count && var_valid(f->type, f->name, n);
}

bool db_parse(db_t *db, const uint8_t *data, size_t length)
{
  db->project_count = 0;
  db->file_count = 0;
  if (length < DB_HEADER_SIZE || memcmp(data, magic, sizeof(magic)) || data[4] != DB_FORMAT) return false;
  uint8_t projects = data[5];
  uint16_t files = (uint16_t)(data[6] | data[7] << 8);
  if (projects > DB_MAX_PROJECTS || files > DB_MAX_FILES ||
      length != DB_HEADER_SIZE + projects * sizeof(db_project_t) + files * sizeof(db_file_t))
    return false;
  memcpy(db->projects, data + DB_HEADER_SIZE, projects * sizeof(db_project_t));
  memcpy(db->files, data + DB_HEADER_SIZE + projects * sizeof(db_project_t), files * sizeof(db_file_t));
  db->project_count = projects;
  db->file_count = files;
  for (uint8_t i = 0; i < projects; i++) {
    db_project_t *p = &db->projects[i];
    if (!terminated(p->slug, sizeof(p->slug)) || !slug_valid(p->slug) || !terminated(p->version, sizeof(p->version)))
      goto bad;
    for (uint8_t j = projects; j < DB_MAX_PROJECTS; j++)
      if (db_needs(p, j)) goto bad;
  }
  for (uint16_t i = 0; i < files; i++)
    if (!file_ok(db, &db->files[i])) goto bad;
  return true;
bad:
  db->project_count = 0;
  db->file_count = 0;
  return false;
}

size_t db_header(const db_t *db, uint8_t *out)
{
  memcpy(out, magic, sizeof(magic));
  out[4] = DB_FORMAT;
  out[5] = db->project_count;
  out[6] = (uint8_t)db->file_count;
  out[7] = (uint8_t)(db->file_count >> 8);
  return DB_HEADER_SIZE + db->project_count * sizeof(db_project_t) + db->file_count * sizeof(db_file_t);
}

uint8_t db_find(const db_t *db, const char *slug)
{
  for (uint8_t i = 0; i < db->project_count; i++)
    if (!strcmp(db->projects[i].slug, slug)) return i;
  return DB_NONE;
}

uint8_t db_add(db_t *db, const char *slug)
{
  uint8_t i = db_find(db, slug);
  if (i != DB_NONE) return i;
  if (db->project_count == DB_MAX_PROJECTS || !slug_valid(slug)) return DB_NONE;
  db_project_t *p = &db->projects[db->project_count];
  memset(p, 0, sizeof(*p));
  strcpy(p->slug, slug);
  return db->project_count++;
}

bool db_needs(const db_project_t *project, uint8_t other) { return project->needs[other >> 3] >> (other & 7) & 1; }

void db_set_need(db_project_t *project, uint8_t other) { project->needs[other >> 3] |= (uint8_t)(1 << (other & 7)); }

static void drop_need(db_project_t *project, uint8_t removed)
{
  for (uint8_t i = removed; i + 1 < DB_MAX_PROJECTS; i++) {
    uint8_t mask = (uint8_t)(1 << (i & 7));
    if (db_needs(project, i + 1)) project->needs[i >> 3] |= mask;
    else project->needs[i >> 3] &= (uint8_t)~mask;
  }
  project->needs[(DB_MAX_PROJECTS - 1) >> 3] &= (uint8_t)~(1 << ((DB_MAX_PROJECTS - 1) & 7));
}

void db_remove(db_t *db, uint8_t project)
{
  uint16_t kept = 0;
  for (uint16_t i = 0; i < db->file_count; i++) {
    db_file_t *f = &db->files[i];
    if (f->project == project) continue;
    if (f->project > project) f->project--;
    db->files[kept++] = *f;
  }
  db->file_count = kept;
  memmove(&db->projects[project], &db->projects[project + 1], (db->project_count - project - 1) * sizeof(db_project_t));
  db->project_count--;
  for (uint8_t i = 0; i < db->project_count; i++) drop_need(&db->projects[i], project);
}

db_file_t *db_find_file(db_t *db, uint8_t type, const uint8_t *name)
{
  for (uint16_t i = 0; i < db->file_count; i++)
    if (var_same(db->files[i].type, db->files[i].name, type, name)) return &db->files[i];
  return NULL;
}

bool db_set_file(db_t *db, uint8_t project, uint8_t type, const uint8_t *name, const uint8_t *sha256)
{
  db_file_t *f = db_find_file(db, type, name);
  if (!f) {
    if (db->file_count == DB_MAX_FILES) return false;
    f = &db->files[db->file_count++];
  }
  f->project = project;
  f->type = type;
  memcpy(f->name, name, VAR_NAME_SIZE);
  memcpy(f->sha256, sha256, SHA_SIZE);
  return true;
}

void db_remove_file(db_t *db, db_file_t *file)
{
  size_t index = (size_t)(file - db->files);
  memmove(file, file + 1, (db->file_count - index - 1) * sizeof(db_file_t));
  db->file_count--;
}

uint8_t db_needed_by(const db_t *db, uint8_t project)
{
  for (uint8_t i = 0; i < db->project_count; i++)
    if (i != project && db_needs(&db->projects[i], project)) return i;
  return DB_NONE;
}

uint8_t db_unneeded(const db_t *db)
{
  uint8_t kept[DB_NEEDS_SIZE] = {0};
  bool grew = true;
  for (uint8_t i = 0; i < db->project_count; i++)
    if (!(db->projects[i].flags & DB_AUTO)) kept[i >> 3] |= (uint8_t)(1 << (i & 7));
  while (grew) {
    grew = false;
    for (uint8_t i = 0; i < db->project_count; i++) {
      if (!(kept[i >> 3] >> (i & 7) & 1)) continue;
      for (uint8_t b = 0; b < DB_NEEDS_SIZE; b++) {
        uint8_t more = db->projects[i].needs[b] & (uint8_t)~kept[b];
        if (more) {
          kept[b] |= more;
          grew = true;
        }
      }
    }
  }
  for (uint8_t i = 0; i < db->project_count; i++)
    if (!(kept[i >> 3] >> (i & 7) & 1)) return i;
  return DB_NONE;
}
