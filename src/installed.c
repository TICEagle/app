#include <string.h>
#include <ti/getcsc.h>

#include "app.h"
#include "net.h"
#include "store.h"
#include "ui.h"
#include "text.h"

#define BATCH 16
#define PATH_SIZE (24 + BATCH * (SLUG_MAX + 1) + 8)
#define TEXT_SIZE 300
#define STATUS_SIZE 40
#define LIST_Y 24
#define ROW_H 25
#define VISIBLE ((UI_FOOTER - LIST_Y) / ROW_H)
#define NAME_X 12
#define EMPTY_Y 96

static uint8_t selected, top;

static bool fetch_updates(void)
{
  static char path[PATH_SIZE];
  updates_t *u = &ws.updates;
  memset(u, 0, sizeof(*u));
  u->db = &db;
  ws_holds = WS_UPDATES;
  if (!db.project_count) return true;
  ui_busy("Installed", "Checking for updates...");
  for (uint8_t start = 0; start < db.project_count; start += BATCH) {
    strcpy(path, "/api/v1/updates?slugs=");
    for (uint8_t i = start; i < db.project_count && i < start + BATCH; i++) {
      if (i > start) strcat(path, ",");
      strcat(path, db.projects[i].slug);
    }
    if (settings.prereleases) strcat(path, "&pre=1");
    json_init(&json, api_updates, u);
    uint8_t result = net_get_json(path, &json);
    if (result != NET_OK) {
      show_net_error("Can't check for updates", result);
      return false;
    }
  }
  return true;
}

static bool has_update(uint8_t i)
{
  update_t *u = &ws.updates.items[i];
  return u->state == UPDATE_LATEST &&
         (!db.projects[i].version[0] || version_compare(u->latest, db.projects[i].version) > 0);
}

static void warn_takedowns(void)
{
  char text[TEXT_SIZE];
  uint8_t count = 0;
  strcpy(text, "These installed projects were taken down from CEagle: ");
  for (uint8_t i = 0; i < db.project_count; i++) {
    if (ws.updates.items[i].state != UPDATE_TAKEN_DOWN) continue;
    if (strlen(text) + SLUG_MAX + 20 < TEXT_SIZE) {
      if (count) strcat(text, ", ");
      strcat(text, db.projects[i].slug);
    }
    count++;
  }
  if (!count) return;
  strcat(text, ". Delete them?");
  if (!ui_confirm(TONE_BAD, "Taken down", text, "Delete")) return;
  for (uint8_t i = 0; i < db.project_count; i++)
    if (ws.updates.items[i].state == UPDATE_TAKEN_DOWN) db.projects[i].flags |= DB_TAKEN_DOWN;
  for (uint8_t i = 0; i < db.project_count;)
    if (db.projects[i].flags & DB_TAKEN_DOWN) uninstall_project(i, false);
    else i++;
  ws_holds = WS_EMPTY;
}

void installed_takedowns(void)
{
  if (db.project_count && fetch_updates()) warn_takedowns();
}

static uint8_t status(char *out, uint8_t i)
{
  update_t *u = &ws.updates.items[i];
  db_project_t *p = &db.projects[i];
  out[0] = 0;
  if (ws_holds != WS_UPDATES) return C_MUTED;
  if (u->state == UPDATE_TAKEN_DOWN) {
    strcpy(out, "Taken down from CEagle");
    return C_BAD;
  }
  if (has_update(i)) {
    text_format(out, STATUS_SIZE, p->version[0] ? "Update to %s" : "Finish install of %s", u->latest);
    return C_OK;
  }
  if (u->state == UPDATE_UNKNOWN) {
    strcpy(out, "Not on this server");
    return C_WARN;
  }
  strcpy(out, p->flags & DB_AUTO ? "Up to date, needed by another project" : "Up to date");
  return C_MUTED;
}

static bool can_update(uint8_t i) { return ws_holds == WS_UPDATES && (has_update(i) || !db.projects[i].version[0]); }

static void draw(void)
{
  static const char *const labels[5] = {"Update", "Remove", NULL, NULL, "Back"};
  const char *footer[5];
  char state[STATUS_SIZE], right[STATUS_SIZE];
  int edge = UI_WIDTH - UI_PAD - 4;
  memcpy(footer, labels, sizeof(footer));
  if (!db.project_count || !can_update(selected)) footer[0] = NULL;
  if (!db.project_count) footer[1] = NULL;
  text_format(right, sizeof(right), db.project_count == 1 ? "%u project" : "%u projects", db.project_count);
  ui_screen("Installed", right);
  if (!db.project_count)
    ui_empty(EMPTY_Y, "Nothing installed yet", "Open a project in Browse and press enter to install it.");
  for (uint8_t i = 0; i < VISIBLE && top + i < db.project_count; i++) {
    uint8_t p = (uint8_t)(top + i);
    const char *version = db.projects[p].version[0] ? db.projects[p].version : "incomplete";
    int y = LIST_Y + i * ROW_H, w = ui_width(version);
    ui_row(y, ROW_H, p == selected);
    ui_bold(NAME_X, y + 4, C_FG, db.projects[p].slug, edge - NAME_X - w - UI_PAD);
    ui_text(edge - w, y + 4, C_MUTED, version, w);
    uint8_t color = status(state, p);
    ui_text(NAME_X, y + 14, color, state, edge - NAME_X);
  }
  ui_scrollbar(LIST_Y, VISIBLE * ROW_H, top, VISIBLE, db.project_count);
  ui_footer(footer);
}

uint8_t installed_run(void)
{
  selected = top = 0;
  ui_backdrop(draw);
  if (fetch_updates()) warn_takedowns();
  else memset(ws.updates.items, 0, sizeof(ws.updates.items));
  for (;;) {
    if (ws_holds != WS_UPDATES) {
      fetch_updates();
      if (selected >= db.project_count) selected = top = 0;
    }
    ui_backdrop(draw);
    draw();
    ui_show();
    switch (ui_key()) {
    case sk_Up:
      if (selected) selected--;
      if (selected < top) top = selected;
      break;
    case sk_Down:
      if (selected + 1 < db.project_count) selected++;
      if (selected >= top + VISIBLE) top++;
      break;
    case sk_Enter:
    case sk_2nd:
    case sk_Yequ:
      if (db.project_count && can_update(selected)) {
        char slug[SLUG_MAX + 1];
        strcpy(slug, db.projects[selected].slug);
        install_project(slug, false);
        ws_holds = WS_EMPTY;
      }
      break;
    case sk_Del:
    case sk_Window:
      if (db.project_count && uninstall_project(selected, true)) ws_holds = WS_EMPTY;
      break;
    case sk_Clear:
    case sk_Graph:
    case sk_Mode: return SCREEN_BROWSE;
    }
  }
}
