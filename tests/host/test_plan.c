#include <string.h>

#include "check.h"
#include "plan.h"

static plan_t plan;
static db_t db;
static uint8_t probe_answer[PLAN_MAX_FILES];

static uint8_t probe(uint8_t type, const uint8_t *name, const uint8_t *sha256)
{
  (void)type;
  (void)sha256;
  return probe_answer[name[1] - 'A'];
}

static void add_file(uint8_t project, uint8_t index, uint8_t type, bool archived, uint32_t size, uint8_t sha)
{
  plan_file_t *f = &plan.files[plan.file_count++];
  memset(f, 0, sizeof(*f));
  f->project = project;
  f->type = type;
  f->name[0] = 'F';
  f->name[1] = (uint8_t)('A' + index);
  f->flags = archived ? FILE_ARCHIVED : 0;
  f->raw_size = size;
  memset(f->sha256, sha, SHA_SIZE);
}

static void setup(void)
{
  memset(&plan, 0, sizeof(plan));
  memset(&db, 0, sizeof(db));
  memset(probe_answer, PROBE_ABSENT, sizeof(probe_answer));
  strcpy(plan.root, "game");
  strcpy(plan.projects[0].slug, "lib");
  strcpy(plan.projects[1].slug, "game");
  plan.project_count = 2;
  add_file(0, 0, VAR_APPVAR, true, 4000, 1);
  add_file(1, 1, VAR_PROTECTED, true, 60000, 2);
  add_file(1, 2, VAR_APPVAR, false, 1000, 3);
}

void test_plan(void)
{
  uint8_t sha[SHA_SIZE];
  setup();
  CHECK(plan_check(&plan, &db, probe) == PLAN_OK);
  CHECK(plan.downloads == 3 && plan.download_bytes == 65000);
  CHECK(plan.ram_needed == PLAN_RAM_RESERVE + 3 * PLAN_VAR_OVERHEAD + 1000 + 60000);
  CHECK(plan.archive_needed == 64000 + 2 * PLAN_VAR_OVERHEAD);

  setup();
  uint8_t g = db_add(&db, "game"), o = db_add(&db, "other");
  memset(sha, 2, SHA_SIZE);
  db_set_file(&db, g, VAR_PROGRAM, plan.files[1].name, sha);
  CHECK(plan_check(&plan, &db, probe) == PLAN_OK);
  CHECK(plan.files[1].flags & FILE_SKIP && plan.files[1].flags & FILE_RECORDED && plan.downloads == 2);
  memset(sha, 9, SHA_SIZE);
  db_set_file(&db, g, VAR_PROGRAM, plan.files[1].name, sha);
  CHECK(plan_check(&plan, &db, probe) == PLAN_OK && plan.files[1].flags & FILE_DOWNLOAD);

  db_set_file(&db, o, VAR_APPVAR, plan.files[2].name, sha);
  CHECK(plan_check(&plan, &db, probe) == PLAN_E_OWNED);
  CHECK(!strcmp(plan.problem, "FC other"));

  setup();
  probe_answer[1] = PROBE_DIFFERENT;
  CHECK(plan_check(&plan, &db, probe) == PLAN_E_EXISTS && !strcmp(plan.problem, "FB"));
  setup();
  probe_answer[1] = PROBE_RESERVED;
  CHECK(plan_check(&plan, &db, probe) == PLAN_E_EXISTS);
  setup();
  probe_answer[1] = PROBE_SELF;
  CHECK(plan_check(&plan, &db, probe) == PLAN_E_EXISTS);
  setup();
  strcpy(plan.projects[1].slug, "ceagle");
  probe_answer[1] = PROBE_SELF;
  CHECK(plan_check(&plan, &db, probe) == PLAN_OK && plan.files[1].flags & FILE_DOWNLOAD);

  setup();
  probe_answer[0] = PROBE_SAME;
  CHECK(plan_check(&plan, &db, probe) == PLAN_OK);
  CHECK(plan.files[0].flags & FILE_ADOPT && !(plan.files[0].flags & FILE_DOWNLOAD));
  CHECK(plan.projects[0].flags & PROJECT_PREEXISTING && plan.downloads == 2);
  setup();
  probe_answer[0] = PROBE_LIBRARY;
  CHECK(plan_check(&plan, &db, probe) == PLAN_OK && plan.replace_asks == 1 && plan.files[0].flags & FILE_ASK);

  setup();
  char slug[8];
  for (int i = 0; i < DB_MAX_PROJECTS - 1; i++) {
    sprintf(slug, "x%d", i + 10);
    db_add(&db, slug);
  }
  CHECK(plan_check(&plan, &db, probe) == PLAN_E_DB_FULL);
  setup();
  db_add(&db, "lib");
  db_add(&db, "game");
  memset(sha, 7, SHA_SIZE);
  for (int i = 0; i < DB_MAX_FILES - 2; i++) {
    uint8_t n[VAR_NAME_SIZE] = {'Z', (uint8_t)('A' + i / 26), (uint8_t)('A' + i % 26)};
    db_set_file(&db, 0, VAR_APPVAR, n, sha);
  }
  CHECK(plan_check(&plan, &db, probe) == PLAN_E_DB_FULL);
}
