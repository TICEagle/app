#include <string.h>
#include <ti/getcsc.h>

#include "app.h"
#include "net.h"
#include "ui.h"
#include "text.h"

#define DETAIL_PATH "/api/v1/projects/%s?view=calculator"
#define PATH_SIZE (sizeof(DETAIL_PATH) + SLUG_MAX)
#define LINE_SIZE 64
#define TITLE_Y 28
#define CONTENT_W (UI_WIDTH - 2 * UI_PAD)
#define STAT_H 30
#define STAT_PAD 6
#define CHIP_H 12
#define CHIP_GAP 4
#define SECTION_GAP 8
#define CAPS 8

detail_t detail;

static const char *const kind_names[] = {"Unknown", "TI-BASIC", "ASM or C", "Library", "AppVar"};

static bool load(const char *slug)
{
  char path[PATH_SIZE];
  memset(&detail, 0, sizeof(detail));
  text_format(path, PATH_SIZE, DETAIL_PATH, slug);
  ui_busy("Project", "Loading project...");
  json_init(&json, api_detail, &detail);
  uint8_t result = net_get_json(path, &json);
  if (result == NET_OK && detail.slug[0]) return true;
  show_net_error("Can't load project", result);
  return false;
}

static void stat(int x, int y, const char *label, const char *value, uint8_t color)
{
  ui_text(x, y + STAT_PAD, C_MUTED, label, CONTENT_W / 2);
  ui_bold(x, y + STAT_PAD + UI_LINE, color, value, CONTENT_W / 2 - STAT_PAD);
}

static int draw_status(int y, uint8_t installed)
{
  const char *version = installed != DB_NONE ? db.projects[installed].version : "";
  bool newer = installed != DB_NONE && detail.latest[0] && (!version[0] || version_compare(detail.latest, version) > 0);
  if (detail.taken_down) {
    ui_fill(UI_PAD, y, CONTENT_W, STAT_H, C_BAD_TINT);
    ui_bold(UI_PAD + STAT_PAD, y + STAT_PAD, C_BAD, "Taken down", CONTENT_W);
    ui_text(UI_PAD + STAT_PAD, y + STAT_PAD + UI_LINE, C_FG, "This project was removed from CEagle.", CONTENT_W);
    return y + STAT_H;
  }
  ui_fill(UI_PAD, y, CONTENT_W, STAT_H, C_SURFACE);
  stat(UI_PAD + STAT_PAD, y, "Latest", detail.latest[0] ? detail.latest : "No release yet",
       detail.latest[0] ? C_FG : C_MUTED);
  if (installed == DB_NONE) stat(UI_WIDTH / 2, y, "On this calculator", "Not installed", C_MUTED);
  else
    stat(UI_WIDTH / 2, y, newer ? "Update available" : "Installed", version[0] ? version : "Incomplete",
         newer || !version[0] ? C_WARN : C_OK);
  return y + STAT_H;
}

static void draw_caps(int y)
{
  int x = UI_PAD;
  ui_text(UI_PAD, y, C_MUTED, "CAPABILITIES", CONTENT_W);
  y += UI_LINE + 2;
  for (uint8_t i = 0; i < CAPS; i++) {
    if (!(detail.caps >> i & 1)) continue;
    int w = ui_width(cap_label(i)) + 2 * CHIP_GAP;
    if (x + w > UI_WIDTH - UI_PAD) {
      x = UI_PAD;
      y += CHIP_H + CHIP_GAP;
    }
    if (y + CHIP_H > UI_FOOTER - CHIP_GAP) return;
    x += ui_chip(x, y, TONE_WARN, false, cap_label(i)) + CHIP_GAP;
  }
}

static void draw(void)
{
  static const char *const labels[5] = {"Install", "Remove", NULL, NULL, "Back"};
  const char *footer[5];
  char line[LINE_SIZE];
  uint8_t installed = db_find(&db, detail.slug);
  int y = TITLE_Y;
  memcpy(footer, labels, sizeof(footer));
  if (installed != DB_NONE) footer[0] = "Update";
  else footer[1] = NULL;
  if (!detail.latest[0] || detail.taken_down) footer[0] = NULL;
  ui_screen("Project", detail.slug);
  y += ui_title(UI_PAD, y, detail.name[0] ? detail.name : detail.slug, CONTENT_W) + 4;
  int x = UI_PAD + ui_chip(UI_PAD, y, TONE_INFO, false, kind_names[detail.kind]) + CHIP_GAP * 2;
  text_format(line, LINE_SIZE, "by %s", detail.owner);
  ui_text(x, y + 2, C_MUTED, line, UI_WIDTH - UI_PAD - x);
  y = draw_status(y + CHIP_H + SECTION_GAP, installed) + SECTION_GAP;
  int room = (UI_FOOTER - y - (detail.caps ? 2 * UI_LINE + CHIP_H : 0)) / UI_LINE;
  if (room > 0) y += ui_wrap(UI_PAD, y, CONTENT_W, (uint8_t)room, C_FG, detail.summary, false) * UI_LINE + SECTION_GAP;
  if (detail.caps) draw_caps(y);
  ui_footer(footer);
}

void detail_run(const char *slug)
{
  if (!load(slug)) return;
  for (;;) {
    uint8_t installed = db_find(&db, detail.slug);
    ui_backdrop(draw);
    draw();
    ui_show();
    switch (ui_key()) {
    case sk_Enter:
    case sk_2nd:
    case sk_Yequ:
      if (detail.latest[0] && !detail.taken_down) install_project(detail.slug, true);
      break;
    case sk_Del:
    case sk_Window:
      if (installed != DB_NONE) uninstall_project(installed, true);
      break;
    case sk_Clear:
    case sk_Graph:
    case sk_Mode: return;
    }
  }
}
