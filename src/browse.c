#include <string.h>
#include <ti/getcsc.h>

#include "app.h"
#include "net.h"
#include "store.h"
#include "ui.h"
#include "text.h"

#define QUERY_SIZE 33
#define PATH_SIZE 192
#define TITLE_SIZE 64
#define RIGHT_SIZE 12
#define STRIP_Y 25
#define STRIP_GAP 4
#define CHIP_PAD 4
#define LIST_Y 43
#define ROW_H 25
#define VISIBLE ((UI_FOOTER - LIST_Y) / ROW_H)
#define NAME_X 12
#define EMPTY_Y 104
#define KINDS 5
#define CATEGORIES 9

static const char *const kind_filters[KINDS] = {"", "basic", "asm", "library", "appvar"};
static const char *const kind_labels[KINDS] = {"All", "BASIC", "ASM/C", "Libraries", "AppVars"};
static const char *const kind_tags[] = {"", "BASIC", "ASM/C", "Library", "AppVar"};
static const char *const category_filters[CATEGORIES] = {
    "", "games", "math", "science", "study", "programming", "graphics", "utilities", "other"};
static const char *const category_labels[CATEGORIES] = {"All",         "Games",    "Math",      "Science", "Study",
                                                        "Programming", "Graphics", "Utilities", "Other"};
static char query[QUERY_SIZE];
static uint8_t kind, category;
static uint16_t page = 1;
static uint8_t selected, top;

static bool load(void)
{
  char path[PATH_SIZE];
  project_list_t *list = &ws.list;
  api_list_path(path, PATH_SIZE, page, kind_filters[kind], category_filters[category], query);
  memset(list, 0, sizeof(*list));
  ws_holds = WS_LIST;
  selected = top = 0;
  ui_busy("Projects", query[0] ? "Searching..." : "Loading projects...");
  json_init(&json, api_list, list);
  uint8_t result = net_get_json(path, &json);
  if (result == NET_OK) return true;
  memset(list, 0, sizeof(*list));
  show_net_error("Can't load projects", result);
  return false;
}

static void draw_strip(void)
{
  int x = UI_PAD;
  for (uint8_t i = 0; i < KINDS; i++) {
    if (i == kind) x += ui_chip(x, STRIP_Y, TONE_ACCENT, true, kind_labels[i]);
    else x += ui_text(x + CHIP_PAD, STRIP_Y + 2, C_MUTED, kind_labels[i], UI_WIDTH) + 2 * CHIP_PAD;
    x += STRIP_GAP;
  }
}

static void draw_item(const list_item_t *item, int y, bool chosen)
{
  int right = UI_WIDTH - UI_PAD - 4, tag = ui_width(kind_tags[item->kind]), version = ui_width(item->latest);
  ui_row(y, ROW_H, chosen);
  ui_bold(NAME_X, y + 4, C_FG, item->name[0] ? item->name : item->slug, right - NAME_X - tag - UI_PAD);
  ui_text(right - tag, y + 4, C_INFO, kind_tags[item->kind], tag);
  ui_text(NAME_X, y + 14, C_MUTED, item->summary[0] ? item->summary : item->slug, right - NAME_X - version - UI_PAD);
  ui_text(right - version, y + 14, C_MUTED, item->latest, version);
}

static void draw(void)
{
  static const char *const labels[5] = {"Search", "Kind", "Updates", "Settings", "Category"};
  char title[TITLE_SIZE], right[RIGHT_SIZE];
  project_list_t *list = &ws.list;
  uint8_t count = ws_holds == WS_LIST ? list->count : 0;
  if (query[0])
    text_format(title, sizeof(title), category ? "Search \"%s\" in %s" : "Search \"%s\"", query,
                category_labels[category]);
  else strcpy(title, category ? category_labels[category] : "Browse");
  text_format(right, sizeof(right), "Page %u", page);
  ui_screen(title, right);
  draw_strip();
  ui_fill(0, LIST_Y - 2, UI_WIDTH, 1, C_SURFACE);
  if (!count)
    ui_empty(EMPTY_Y, "No projects found",
             query[0] ? "Nothing matches this search. Press y= to search again."
                      : "Press window for another kind or graph for another category.");
  for (uint8_t i = 0; i < VISIBLE && top + i < count; i++)
    draw_item(&list->items[top + i], LIST_Y + i * ROW_H, top + i == selected);
  ui_scrollbar(LIST_Y, VISIBLE * ROW_H, top, VISIBLE, count);
  ui_footer(labels);
}

static void move(int delta)
{
  project_list_t *list = &ws.list;
  if (!list->count) return;
  if (delta < 0 && selected) selected--;
  if (delta > 0 && selected + 1 < list->count) selected++;
  if (selected < top) top = selected;
  if (selected >= top + VISIBLE) top = (uint8_t)(selected - VISIBLE + 1);
}

uint8_t browse_run(void)
{
  ui_backdrop(draw);
  if (ws_holds != WS_LIST) load();
  for (;;) {
    ui_backdrop(draw);
    draw();
    ui_show();
    switch (ui_key()) {
    case sk_Up: move(-1); break;
    case sk_Down: move(1); break;
    case sk_Right:
      if (ws.list.more) {
        page++;
        load();
      }
      break;
    case sk_Left:
      if (page > 1) {
        page--;
        load();
      }
      break;
    case sk_Enter:
    case sk_2nd:
      if (ws.list.count) {
        char slug[SLUG_MAX + 1];
        strcpy(slug, ws.list.items[selected].slug);
        detail_run(slug);
        if (ws_holds != WS_LIST) load();
      }
      break;
    case sk_Yequ:
      if (ui_edit("Search", "Type part of a project name.", query, sizeof(query))) {
        page = 1;
        load();
      }
      break;
    case sk_Window:
      kind = (uint8_t)((kind + 1) % KINDS);
      page = 1;
      load();
      break;
    case sk_Graph: {
      uint8_t picked = ui_pick("Category", category_labels, CATEGORIES, category);
      if (picked != category) {
        category = picked;
        page = 1;
        load();
      }
      break;
    }
    case sk_Zoom: return SCREEN_INSTALLED;
    case sk_Trace: return SCREEN_SETTINGS;
    case sk_Clear:
    case sk_Mode: return SCREEN_QUIT;
    }
  }
}
