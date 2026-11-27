#include <string.h>
#include <ti/getcsc.h>

#include "app.h"
#include "text.h"
#include "ui.h"

#define TOP_Y 26
#define ROW_H 25
#define NAME_X 12
#define CHIP_H 12
#define CHIP_GAP 4
#define CAPS 8
#define LINE_SIZE 64
#define KILOBYTE 1024ul

static const plan_t *shown;
static uint8_t order[PLAN_MAX_PROJECTS];
static uint8_t selected, top, list_y;

typedef struct {
  uint8_t downloads;
  uint32_t bytes;
} totals_t;

static totals_t count(uint8_t project)
{
  totals_t t = {0, 0};
  for (uint8_t i = 0; i < shown->file_count; i++) {
    const plan_file_t *f = &shown->files[i];
    if (f->project != project || !(f->flags & FILE_DOWNLOAD)) continue;
    t.downloads++;
    t.bytes += f->raw_size;
  }
  return t;
}

static void size_text(char *out, uint32_t bytes)
{
  if (bytes < KILOBYTE) text_format(out, LINE_SIZE, "%lu B", bytes);
  else text_format(out, LINE_SIZE, "%lu KB", (bytes + KILOBYTE - 1) / KILOBYTE);
}

static uint8_t status(char *out, const plan_project_t *p)
{
  const char *installed = p->db != DB_NONE ? db.projects[p->db].version : NULL;
  if (!installed) {
    strcpy(out, p->flags & PROJECT_PREEXISTING ? "Already on this calculator" : "New");
    return p->flags & PROJECT_PREEXISTING ? C_MUTED : C_OK;
  }
  if (!installed[0] || strcmp(installed, p->version)) {
    text_format(out, LINE_SIZE, installed[0] ? "Update from %s" : "Finish install", installed);
    return C_INFO;
  }
  strcpy(out, "Installed");
  return C_MUTED;
}

static int draw_caps(int y)
{
  int x = UI_PAD;
  for (uint8_t i = 0; i < CAPS; i++) {
    if (!(shown->caps >> i & 1)) continue;
    int w = ui_width(cap_label(i)) + 2 * CHIP_GAP;
    if (x + w > UI_WIDTH - UI_PAD) {
      x = UI_PAD;
      y += CHIP_H + CHIP_GAP;
    }
    x += ui_chip(x, y, TONE_WARN, false, cap_label(i)) + CHIP_GAP;
  }
  return y + CHIP_H + CHIP_GAP;
}

static void draw_project(uint8_t index, int y)
{
  char state[LINE_SIZE], files[LINE_SIZE];
  const plan_project_t *p = &shown->projects[order[index]];
  totals_t t = count(order[index]);
  int edge = UI_WIDTH - UI_PAD - 4, version = ui_width(p->version);
  ui_row(y, ROW_H, index == selected);
  ui_bold(NAME_X, y + 4, C_FG, p->slug, edge - NAME_X - version - UI_PAD);
  ui_text(edge - version, y + 4, C_MUTED, p->version, version);
  uint8_t color = status(state, p);
  int w = ui_text(NAME_X, y + 14, color, state, edge - NAME_X);
  if (t.downloads) {
    char size[LINE_SIZE];
    size_text(size, t.bytes);
    text_format(files, LINE_SIZE, "%u %s, %s", t.downloads, t.downloads == 1 ? "file" : "files", size);
  } else strcpy(files, "nothing to download");
  ui_text(edge - ui_width(files), y + 14, C_MUTED, files, edge - NAME_X - w - UI_PAD);
}

static uint8_t visible(void) { return (uint8_t)((UI_FOOTER - list_y) / ROW_H); }

static void draw(void)
{
  static const char *const labels[5] = {"Install", NULL, NULL, NULL, "Cancel"};
  char line[LINE_SIZE], right[LINE_SIZE], size[LINE_SIZE];
  int y = TOP_Y;
  text_format(right, LINE_SIZE, "%s %s", shown->root, shown->version);
  ui_screen("Review", right);
  size_text(size, shown->download_bytes);
  text_format(line, LINE_SIZE, "%u %s, %u %s to download, %s", shown->project_count,
              shown->project_count == 1 ? "project" : "projects", shown->downloads,
              shown->downloads == 1 ? "file" : "files", size);
  ui_bold(UI_PAD, y, C_FG, line, UI_WIDTH - 2 * UI_PAD);
  y += UI_LINE + 4;
  if (shown->caps) {
    ui_text(UI_PAD, y, C_MUTED, "IT CAN", UI_WIDTH);
    y = draw_caps(y + UI_LINE + 2);
  }
  ui_text(UI_PAD, y, C_MUTED, shown->project_count == 1 ? "PROJECT" : "PROJECTS", UI_WIDTH);
  list_y = (uint8_t)(y + UI_LINE + 2);
  for (uint8_t i = 0; i < visible() && top + i < shown->project_count; i++)
    draw_project((uint8_t)(top + i), list_y + i * ROW_H);
  ui_scrollbar(list_y, visible() * ROW_H, top, visible(), shown->project_count);
  ui_footer(labels);
}

static void sort(void)
{
  uint8_t n = 0, root = 0;
  for (uint8_t i = 0; i < shown->project_count; i++)
    if (!strcmp(shown->projects[i].slug, shown->root)) root = i;
  order[n++] = root;
  for (uint8_t i = 0; i < shown->project_count; i++)
    if (i != root) order[n++] = i;
}

bool review_plan(const plan_t *plan)
{
  shown = plan;
  selected = top = 0;
  sort();
  for (;;) {
    ui_backdrop(draw);
    draw();
    ui_show();
    switch (ui_key()) {
    case sk_Up:
      if (selected) selected--;
      if (selected < top) top = selected;
      break;
    case sk_Down:
      if (selected + 1 < plan->project_count) selected++;
      if (selected >= top + visible()) top++;
      break;
    case sk_Enter:
    case sk_2nd:
    case sk_Yequ: return true;
    case sk_Clear:
    case sk_Graph:
    case sk_Mode: return false;
    }
  }
}
