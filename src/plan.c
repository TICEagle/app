#include <string.h>

#include "plan.h"

static uint8_t problem(plan_t *plan, uint8_t error, const plan_file_t *f, const char *owner)
{
  size_t n;
  var_display(plan->problem, f->type, f->name);
  if (owner) {
    n = strlen(plan->problem);
    plan->problem[n] = ' ';
    strcpy(plan->problem + n + 1, owner);
  }
  return plan->error = error;
}

static uint8_t classify(plan_t *plan, db_t *db, plan_file_t *f, var_probe_t probe)
{
  plan_project_t *p = &plan->projects[f->project];
  db_file_t *owned = db_find_file(db, f->type, f->name);
  if (owned) {
    const char *owner = db->projects[owned->project].slug;
    if (strcmp(owner, p->slug)) return problem(plan, PLAN_E_OWNED, f, owner);
    f->flags |= FILE_RECORDED;
    f->flags |= memcmp(owned->sha256, f->sha256, SHA_SIZE) ? FILE_DOWNLOAD : FILE_SKIP;
    return PLAN_OK;
  }
  switch (probe(f->type, f->name, f->sha256)) {
  case PROBE_ABSENT: f->flags |= FILE_DOWNLOAD; return PLAN_OK;
  case PROBE_SAME:
    f->flags |= FILE_ADOPT;
    p->flags |= PROJECT_PREEXISTING;
    return PLAN_OK;
  case PROBE_LIBRARY:
    f->flags |= FILE_DOWNLOAD | FILE_ASK;
    p->flags |= PROJECT_PREEXISTING;
    plan->replace_asks++;
    var_display(plan->problem, f->type, f->name);
    return PLAN_OK;
  case PROBE_SELF:
    if (strcmp(p->slug, "ceagle")) break;
    f->flags |= FILE_DOWNLOAD;
    p->flags |= PROJECT_PREEXISTING;
    return PLAN_OK;
  }
  return problem(plan, PLAN_E_EXISTS, f, NULL);
}

uint8_t plan_check(plan_t *plan, db_t *db, var_probe_t probe)
{
  uint32_t largest_archived = 0;
  uint16_t new_files = 0;
  uint8_t new_projects = 0;
  plan->ram_needed = PLAN_RAM_RESERVE;
  plan->archive_needed = 0;
  plan->download_bytes = 0;
  plan->downloads = 0;
  plan->replace_asks = 0;
  for (uint8_t i = 0; i < plan->project_count; i++) {
    plan->projects[i].db = db_find(db, plan->projects[i].slug);
    plan->projects[i].flags = 0;
    new_projects += plan->projects[i].db == DB_NONE;
  }
  for (uint8_t i = 0; i < plan->file_count; i++) {
    plan_file_t *f = &plan->files[i];
    f->flags &= FILE_ARCHIVED;
    if (classify(plan, db, f, probe)) return plan->error;
    if (!(f->flags & FILE_RECORDED) && !(f->flags & FILE_SKIP)) new_files++;
    if (!(f->flags & FILE_DOWNLOAD)) continue;
    plan->downloads++;
    plan->download_bytes += f->raw_size;
    plan->ram_needed += PLAN_VAR_OVERHEAD;
    if (!(f->flags & FILE_ARCHIVED)) plan->ram_needed += f->raw_size;
    else {
      plan->archive_needed += f->raw_size + PLAN_VAR_OVERHEAD;
      if (f->raw_size > largest_archived) largest_archived = f->raw_size;
    }
  }
  plan->ram_needed += largest_archived;
  if (db->project_count + new_projects > DB_MAX_PROJECTS || db->file_count + new_files > DB_MAX_FILES)
    return plan->error = PLAN_E_DB_FULL;
  return PLAN_OK;
}
