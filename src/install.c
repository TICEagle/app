#include <string.h>

#include "app.h"
#include "dl.h"
#include "net.h"
#include "plan.h"
#include "store.h"
#include "ui.h"
#include "text.h"

#define PATH_SIZE 160
#define TEXT_SIZE 400
#define KILOBYTE 1024u
#define TITLE_SIZE 32

json_t json;
static dl_t dl;
static char text[TEXT_SIZE];

static const char *const cap_labels[] = {"Writes to flash memory",
                                         "Deletes variables",
                                         "Can clear RAM",
                                         "Uses USB",
                                         "Uses the network",
                                         "Runs assembly code from TI-BASIC",
                                         "Sends or receives over the link port",
                                         "Has other capabilities"};

const char *cap_label(uint8_t bit) { return cap_labels[bit]; }

void show_net_error(const char *title, uint8_t result)
{
  switch (result) {
  case NET_E_CONNECT:
    text_format(text, TEXT_SIZE, "Can't reach %s (%s). Check the connection and the server address in Settings.",
                settings.server, net_message);
    break;
  case NET_E_HTTP: text_format(text, TEXT_SIZE, "%s", net_message); break;
  case NET_E_CANCEL: strcpy(text, "Stopped."); break;
  case NET_E_TOO_LONG: strcpy(text, "The server sent more data than expected."); break;
  case NET_E_SHORT: strcpy(text, "The connection closed before the end. Nothing else was changed. Try again."); break;
  default: strcpy(text, "The server sent an answer CEagle can't read.");
  }
  ui_message(TONE_BAD, title, text);
}

static void save_records(void)
{
  if (!store_save_db(&db))
    ui_message(TONE_WARN, "Not enough memory",
               "CEagle could not save its list of installed projects. Free some RAM, then try again.");
}

static void plan_error(const plan_t *plan)
{
  char name[sizeof(plan->problem)];
  char *owner;
  strcpy(name, plan->problem);
  owner = strchr(name, ' ');
  if (owner) *owner++ = 0;
  switch (plan->error) {
  case PLAN_E_TOO_MANY_FILES:
    text_format(text, TEXT_SIZE, "This install has more files than CEagle can handle at once (at most %u).",
                PLAN_MAX_FILES);
    break;
  case PLAN_E_TOO_MANY_PROJECTS:
    text_format(text, TEXT_SIZE, "This install needs more than %u projects.", PLAN_MAX_PROJECTS);
    break;
  case PLAN_E_TOO_LARGE:
    text_format(text, TEXT_SIZE, "%s is larger than a calculator variable can be (at most %u bytes).", name,
                VAR_DATA_MAX);
    break;
  case PLAN_E_VERSION:
    text_format(text, TEXT_SIZE, "A version number in this install is longer than %u characters.", VERSION_MAX);
    break;
  case PLAN_E_DUPLICATE: text_format(text, TEXT_SIZE, "%s appears twice in this install.", name); break;
  case PLAN_E_OWNED:
    text_format(text, TEXT_SIZE, "%s belongs to %s, which is installed. CEagle won't overwrite it.", name, owner);
    break;
  case PLAN_E_EXISTS:
    text_format(
        text, TEXT_SIZE,
        "A variable named %s is already on your calculator and wasn't installed by CEagle. Rename or delete it, "
        "then try again.",
        name);
    break;
  case PLAN_E_DB_FULL:
    text_format(text, TEXT_SIZE,
                "CEagle can keep track of at most %u projects and %u files. Uninstall something first.",
                DB_MAX_PROJECTS, DB_MAX_FILES);
    break;
  default: strcpy(text, "The server sent an install list CEagle can't read.");
  }
  ui_message(TONE_BAD, "Can't install", text);
}

static bool space_ok(const plan_t *plan)
{
  uint32_t ram = store_free_ram(), archive = store_free_archive();
  if (ram < plan->ram_needed) {
    text_format(text, TEXT_SIZE,
                "Not enough free RAM. This install needs %lu KB and %lu KB are free. Archive or delete some variables, "
                "then try again.",
                (unsigned long)((plan->ram_needed + KILOBYTE - 1) / KILOBYTE), (unsigned long)(ram / KILOBYTE));
    ui_message(TONE_WARN, "Not enough memory", text);
    return false;
  }
  if (archive < plan->archive_needed) {
    text_format(
        text, TEXT_SIZE,
        "Not enough free archive. This install needs %lu KB and %lu KB are free. Delete some archived variables "
        "or apps, then try again.",
        (unsigned long)((plan->archive_needed + KILOBYTE - 1) / KILOBYTE), (unsigned long)(archive / KILOBYTE));
    ui_message(TONE_WARN, "Not enough memory", text);
    return false;
  }
  return true;
}

static bool confirmed(const plan_t *plan)
{
  if (plan->replace_asks) {
    text_format(text, TEXT_SIZE,
                "%s is already on your calculator and is different from the copy on CEagle. Replace it?",
                plan->problem);
    if (!ui_confirm(TONE_WARN, "Replace library", text, "Replace")) return false;
  }
  return (plan->project_count == 1 && !plan->caps) || review_plan(plan);
}

static bool dl_sink(void *arg, const uint8_t *data, size_t length) { return dl_feed(arg, data, length); }

static const char *dl_problem(uint8_t error)
{
  switch (error) {
  case DL_E_HASH: return "did not match its checksum";
  case DL_E_SHORT: return "stopped before the end";
  case DL_E_LONG: return "was larger than expected";
  case DL_E_PREFIX: return "is not a valid calculator variable";
  }
  return "did not fit in free RAM";
}

static bool download(plan_t *plan, plan_file_t *f, uint8_t number)
{
  char path[PATH_SIZE], name[VAR_DISPLAY_SIZE], title[TITLE_SIZE];
  plan_project_t *p = &plan->projects[f->project];
  var_display(name, f->type, f->name);
  text_format(title, TITLE_SIZE, "Installing file %u of %u", number, plan->downloads);
  text_format(text, TEXT_SIZE, "%s from %s %s", name, p->slug, p->version);
  ui_busy(title, text);
  text_format(path, PATH_SIZE, "/api/v1/dl/%s/%s/%s?format=raw", p->slug, p->version, plan->pool + f->filename);
  dl_init(&dl, f->type, f->raw_size, f->sha256, store_open, f);
#ifdef CEAGLE_TEST
  test_log(name);
#endif
  uint8_t result = net_get(path, dl_sink, &dl, f->raw_size, f->raw_size);
  bool ok = result == NET_OK && dl_finish(&dl);
#ifdef CEAGLE_TEST
  test_log(ok ? "downloaded" : "failed");
#endif
  store_finish(f, ok);
#ifdef CEAGLE_TEST
  test_log("finished");
#endif
  if (ok) return true;
  if (f->flags & FILE_RECORDED && !store_exists(f->type, f->name))
    db_remove_file(&db, db_find_file(&db, f->type, f->name));
  save_records();
  if (dl.error) {
    text_format(text, TEXT_SIZE, "%s %s, so CEagle deleted it. Nothing else was changed. Try again later.", name,
                dl_problem(dl.error));
    ui_message(TONE_BAD, "Install stopped", text);
  } else show_net_error("Install stopped", result);
  return false;
}

static void remove_stale(plan_t *plan, uint8_t plan_index, uint8_t project)
{
  for (uint16_t i = 0; i < db.file_count;) {
    db_file_t *f = &db.files[i];
    bool kept = f->project != project;
    for (uint8_t j = 0; !kept && j < plan->file_count; j++)
      kept =
          plan->files[j].project == plan_index && var_same(plan->files[j].type, plan->files[j].name, f->type, f->name);
    if (kept) {
      i++;
      continue;
    }
    store_delete(f->type, f->name);
    db_remove_file(&db, f);
  }
}

static void drop_empty(uint8_t project)
{
  if (db.projects[project].version[0]) return;
  for (uint16_t i = 0; i < db.file_count; i++)
    if (db.files[i].project == project) return;
  db_remove(&db, project);
}

static bool execute(plan_t *plan, bool chosen)
{
  uint8_t number = 0, root = DB_NONE;
  for (uint8_t i = 0; i < plan->project_count; i++) {
    plan_project_t *p = &plan->projects[i];
    bool is_root = !strcmp(p->slug, plan->root), fresh = db_find(&db, p->slug) == DB_NONE;
    uint8_t project = db_add(&db, p->slug);
    if (project == DB_NONE) return false;
    p->db = project;
    if (fresh && !is_root && !(p->flags & PROJECT_PREEXISTING)) db.projects[project].flags = DB_AUTO;
    if (is_root) {
      root = project;
      if (chosen) db.projects[project].flags &= (uint8_t)~DB_AUTO;
    }
    for (uint8_t j = 0; j < plan->file_count; j++) {
      plan_file_t *f = &plan->files[j];
      if (f->project != i || f->flags & FILE_SKIP) continue;
      if (f->flags & FILE_DOWNLOAD && !download(plan, f, ++number)) {
        drop_empty(project);
        save_records();
        return false;
      }
      db_set_file(&db, project, f->type, f->name, f->sha256);
    }
    remove_stale(plan, i, project);
    strcpy(db.projects[project].version, p->version);
    save_records();
  }
  if (root != DB_NONE) {
    memset(db.projects[root].needs, 0, DB_NEEDS_SIZE);
    for (uint8_t i = 0; i < plan->project_count; i++)
      if (plan->projects[i].db != root) db_set_need(&db.projects[root], plan->projects[i].db);
  }
  save_records();
  return true;
}

bool install_project(const char *slug, bool chosen)
{
  char path[PATH_SIZE];
  plan_t *plan = &ws.plan;
  memset(plan, 0, sizeof(*plan));
  ws_holds = WS_PLAN;
  text_format(path, PATH_SIZE, "/api/v1/projects/%s/install%s", slug, settings.prereleases ? "?pre=1" : "");
  ui_busy("Install", "Getting the list of files...");
  json_init(&json, api_install, plan);
  uint8_t result = net_get_json(path, &json);
  if (plan->error || (result == NET_OK && !api_install_done(plan))) {
    plan_error(plan);
    return false;
  }
  if (result != NET_OK) {
    show_net_error("Can't install", result);
    return false;
  }
  if (plan_check(plan, &db, store_probe)) {
    plan_error(plan);
    return false;
  }
  if (!space_ok(plan) || !confirmed(plan) || !execute(plan, chosen)) return false;
  if (!strcmp(plan->root, "ceagle"))
    strcpy(text, "CEagle was updated. Quit and start it again to use the new version.");
  else text_format(text, TEXT_SIZE, "%s %s is installed.", plan->root, plan->version);
  ui_message(TONE_OK, "Installed", text);
  return true;
}

static void remove_files(uint8_t project)
{
  for (uint16_t i = 0; i < db.file_count; i++)
    if (db.files[i].project == project) store_delete(db.files[i].type, db.files[i].name);
  db_remove(&db, project);
}

bool uninstall_project(uint8_t project, bool ask)
{
  uint8_t needer = db_needed_by(&db, project), unneeded;
  if (ask) {
    if (needer != DB_NONE)
      text_format(text, TEXT_SIZE, "%s needs %s. Uninstall %s anyway?", db.projects[needer].slug,
                  db.projects[project].slug, db.projects[project].slug);
    else text_format(text, TEXT_SIZE, "Uninstall %s and delete its files?", db.projects[project].slug);
    if (!ui_confirm(TONE_BAD, "Uninstall", text, "Uninstall")) return false;
  }
  remove_files(project);
  while ((unneeded = db_unneeded(&db)) != DB_NONE) remove_files(unneeded);
  save_records();
  return true;
}
