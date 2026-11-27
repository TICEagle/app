#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "db.h"

static db_t db, copy;
static uint8_t buf[DB_SIZE_MAX + 16];

static size_t db_serialize(const db_t *d, uint8_t *out)
{
  size_t size = db_header(d, out), projects = d->project_count * sizeof(db_project_t);
  memcpy(out + DB_HEADER_SIZE, d->projects, projects);
  memcpy(out + DB_HEADER_SIZE + projects, d->files, d->file_count * sizeof(db_file_t));
  return size;
}

static void name(uint8_t *out, const char *text)
{
  memset(out, 0, VAR_NAME_SIZE);
  memcpy(out, text, strlen(text));
}

void test_db(void)
{
  uint8_t n[VAR_NAME_SIZE], sha[SHA_SIZE] = {1};
  memset(&db, 0, sizeof(db));
  CHECK(slug_valid("ab") && slug_valid("a-b") && !slug_valid("-ab") && !slug_valid("a") && !slug_valid("A1"));
  uint8_t lib = db_add(&db, "graphx"), prog = db_add(&db, "gdash"), other = db_add(&db, "tetris");
  CHECK(lib == 0 && prog == 1 && other == 2 && db_add(&db, "gdash") == 1 && db_add(&db, "Bad") == DB_NONE);
  db.projects[lib].flags = DB_AUTO;
  db_set_need(&db.projects[prog], lib);
  name(n, "GRAPHX");
  CHECK(db_set_file(&db, lib, VAR_APPVAR, n, sha));
  name(n, "GDASH");
  CHECK(db_set_file(&db, prog, VAR_PROTECTED, n, sha));
  CHECK(db_find_file(&db, VAR_PROGRAM, n)->project == prog);
  name(n, "TETRIS");
  CHECK(db_set_file(&db, other, VAR_PROGRAM, n, sha));
  CHECK(db_needed_by(&db, lib) == prog);
  CHECK(db_unneeded(&db) == DB_NONE);

  size_t size = db_serialize(&db, buf);
  CHECK(db_parse(&copy, buf, size) && copy.project_count == 3 && copy.file_count == 3);
  CHECK(!db_parse(&copy, buf, size - 1));
  buf[DB_HEADER_SIZE] = 'A';
  CHECK(!db_parse(&copy, buf, size));
  for (int round = 0; round < 50000; round++) {
    size = db_serialize(&db, buf);
    for (int k = rng() % 3; k >= 0; k--) buf[rng() % size] = (uint8_t)rng();
    if (db_parse(&copy, buf, size))
      for (uint16_t i = 0; i < copy.file_count; i++) CHECK(copy.files[i].project < copy.project_count);
  }

  db_remove(&db, 0);
  CHECK(db.project_count == 2 && db.file_count == 2 && db.files[0].project == 0 && db.files[1].project == 1);
  CHECK(!db_needs(&db.projects[0], 0) && !db_needs(&db.projects[0], 1));

  memset(&db, 0, sizeof(db));
  uint8_t a = db_add(&db, "aa"), b = db_add(&db, "bb"), c = db_add(&db, "cc"), d = db_add(&db, "dd");
  db.projects[b].flags = db.projects[c].flags = db.projects[d].flags = DB_AUTO;
  db_set_need(&db.projects[a], b);
  db_set_need(&db.projects[b], c);
  db_set_need(&db.projects[c], b);
  CHECK(db_unneeded(&db) == d);
  db_remove(&db, d);
  CHECK(db_unneeded(&db) == DB_NONE);
  db_remove(&db, a);
  CHECK(db_needs(&db.projects[0], 1) && db_needs(&db.projects[1], 0));
  uint8_t gone = db_unneeded(&db);
  CHECK(gone == 0);
  db_remove(&db, gone);
  CHECK(db_unneeded(&db) == 0);
  db_remove(&db, 0);
  CHECK(db.project_count == 0);

  memset(&db, 0, sizeof(db));
  char slug[8];
  for (int i = 0; i < DB_MAX_PROJECTS; i++) {
    sprintf(slug, "p%d", i + 10);
    CHECK(db_add(&db, slug) == i);
  }
  CHECK(db_add(&db, "full") == DB_NONE);
  db_set_need(&db.projects[0], DB_MAX_PROJECTS - 1);
  db_set_need(&db.projects[0], 40);
  db_remove(&db, 3);
  CHECK(db_needs(&db.projects[0], 39) && db_needs(&db.projects[0], DB_MAX_PROJECTS - 2) &&
        !db_needs(&db.projects[0], 40));
  for (int i = 0; i < DB_MAX_FILES; i++) {
    memset(n, 0, sizeof(n));
    n[0] = 'A';
    n[1] = (uint8_t)('A' + i / 26);
    n[2] = (uint8_t)('A' + i % 26);
    CHECK(db_set_file(&db, (uint8_t)(i % db.project_count), VAR_APPVAR, n, sha));
  }
  name(n, "EXTRA");
  CHECK(!db_set_file(&db, 0, VAR_APPVAR, n, sha));
  size = db_serialize(&db, buf);
  CHECK(size <= DB_SIZE_MAX && db_parse(&copy, buf, size));
  printf("db: largest CEAGLEDB is %zu bytes (%d projects, %d files)\n", (size_t)DB_SIZE_MAX, DB_MAX_PROJECTS,
         DB_MAX_FILES);
}
