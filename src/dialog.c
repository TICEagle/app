#include <graphx.h>
#include <string.h>
#include <ti/getcsc.h>

#include "text.h"
#include "ui.h"

#define PANEL_X 16
#define PANEL_W 288
#define PAD 12
#define TEXT_W (PANEL_W - 2 * PAD)
#define STRIPE 3
#define TITLE_Y 12
#define BODY_Y 30
#define BOTTOM 12
#define GAP 8
#define MAX_LINES 12
#define ACTION_H 12
#define ACTIONS (ACTION_H + GAP)
#define ACTION_GAP 14
#define BAR_H 6
#define PROGRESS_H (BAR_H + 4 + 8)
#define PROGRESS (PROGRESS_H + GAP)
#define LOADER_W 10
#define LOADER_GAP 4
#define FIELD_H 20
#define FIELD (FIELD_H + GAP)
#define CARET_W 2
#define KILOBYTE 1024ul
#define PERCENT 100ul
#define LABEL_SIZE 32
#define KEY_TABLE 64
#define WORK_SIZE 80

static int bar_y;

static int panel(uint8_t tone, const char *title, const char *text, int extra)
{
  uint8_t lines = ui_wrap(0, -1, TEXT_W, MAX_LINES, C_FG, text, false);
  int h = BODY_Y + lines * UI_LINE + (lines ? GAP : 0) + extra + BOTTOM - GAP;
  int y = (UI_HEIGHT - h) / 2;
  ui_dim();
  ui_fill(PANEL_X - 1, y - 1, PANEL_W + 2, h + 2, C_LINE);
  ui_fill(PANEL_X, y, PANEL_W, h, C_SURFACE);
  ui_fill(PANEL_X, y, PANEL_W, STRIPE, C_ACCENT + tone);
  ui_bold(PANEL_X + PAD, y + TITLE_Y, C_FG, title, TEXT_W);
  ui_wrap(PANEL_X + PAD, y + BODY_Y, TEXT_W, lines, C_FG, text, false);
  return y + BODY_Y + lines * UI_LINE + (lines ? GAP : 0);
}

void ui_message(uint8_t tone, const char *title, const char *text)
{
  int y = panel(tone, title, text, ACTIONS);
  bar_y = 0;
  ui_keycap(PANEL_X + PAD, y, "enter", "OK", C_ACCENT + tone);
  ui_show();
  ui_key();
}

bool ui_confirm(uint8_t tone, const char *title, const char *text, const char *yes)
{
  int y = panel(tone, title, text, ACTIONS);
  int x = PANEL_X + PAD;
  bar_y = 0;
  x += ui_keycap(x, y, "enter", yes, C_ACCENT + tone) + ACTION_GAP;
  ui_keycap(x, y, "clear", "Cancel", C_MUTED);
  ui_show();
  for (;;) {
    uint8_t key = ui_key();
    if (key == sk_Enter || key == sk_2nd) return true;
    if (key == sk_Clear || key == sk_Del) return false;
  }
}

void ui_busy(const char *title, const char *text)
{
  int y = panel(TONE_ACCENT, title, text, PROGRESS + ACTIONS);
  bar_y = y;
  for (int i = 0; i < 3; i++) ui_fill(PANEL_X + PAD + i * (LOADER_W + LOADER_GAP), y, LOADER_W, BAR_H, C_LINE);
  ui_keycap(PANEL_X + PAD, y + PROGRESS, "clear", "Stop", C_MUTED);
  ui_show();
}

void ui_progress(uint32_t done, uint32_t total)
{
  char label[LABEL_SIZE], percent[LABEL_SIZE];
  int x = PANEL_X + PAD;
  if (!total || !bar_y) return;
  if (done > total) done = total;
  ui_fill(x, bar_y, TEXT_W, PROGRESS_H, C_SURFACE);
  ui_fill(x, bar_y, TEXT_W, BAR_H, C_LINE);
  ui_fill(x, bar_y, (int)(done * TEXT_W / total), BAR_H, C_ACCENT);
  text_format(label, LABEL_SIZE, "%lu of %lu KB", done / KILOBYTE, (total + KILOBYTE - 1) / KILOBYTE);
  text_format(percent, LABEL_SIZE, "%lu", done * PERCENT / total);
  strcat(percent, "%");
  ui_text(x, bar_y + BAR_H + 4, C_MUTED, label, TEXT_W);
  ui_bold(x + TEXT_W - ui_width(percent) - 1, bar_y + BAR_H + 4, C_FG, percent, TEXT_W);
  gfx_BlitRectangle(gfx_buffer, (uint24_t)x, (uint8_t)bar_y, TEXT_W, PROGRESS_H);
}

static char key_char(uint8_t key, bool letters)
{
  static const char alpha[KEY_TABLE] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, 'w', 'r', 'm', 'h', 0,   0,   0,   0, 'v', 'q', 'l',
      'g', 0,   0,   0,   'z', 'u', 'p', 'k', 'f', 'c', 0, ' ', 'y', 't', 'o', 'j', 'e', 'b', 0, 0,   'x', 's',
      'n', 'i', 'd', 'a', 0,   0,   0,   0,   0,   0,   0, 0,   0,   0,   0,   0,   0,   0,   0, 0,
  };
  static const uint8_t digit_keys[] = {sk_0, sk_1, sk_2, sk_3, sk_4, sk_5, sk_6, sk_7, sk_8, sk_9};
  if (key == sk_DecPnt) return letters ? ':' : '.';
  if (letters) return key < KEY_TABLE ? alpha[key] : 0;
  for (uint8_t i = 0; i < sizeof(digit_keys); i++)
    if (digit_keys[i] == key) return (char)('0' + i);
  if (key == sk_Div) return '/';
  if (key == sk_Sub || key == sk_Chs) return '-';
  return 0;
}

static void draw_input(int y, const char *work, bool letters)
{
  int x = PANEL_X + PAD, mode_w = ui_width("abc") + 8, room = TEXT_W - 2 * PAD - mode_w - CARET_W;
  const char *shown = work;
  while (ui_width(shown) > room) shown++;
  ui_fill(x, y, TEXT_W, FIELD + 2 * ACTIONS, C_SURFACE);
  ui_fill(x, y, TEXT_W, FIELD_H, C_ACCENT);
  ui_fill(x + 1, y + 1, TEXT_W - 2, FIELD_H - 2, C_BG);
  int w = ui_text(x + PAD / 2, y + 6, C_FG, shown, room);
  ui_fill(x + PAD / 2 + w + 1, y + 4, CARET_W, FIELD_H - 8, C_ACCENT);
  ui_chip(x + TEXT_W - mode_w - 4, y + 4, TONE_ACCENT, true, letters ? "abc" : "123");
  int row = y + FIELD;
  int col = x + ui_keycap(x, row, "enter", "Done", C_ACCENT) + ACTION_GAP;
  ui_keycap(col, row, "clear", work[0] ? "Clear" : "Cancel", C_MUTED);
  col = x + ui_keycap(x, row + ACTIONS, "alpha", letters ? "To 123" : "To abc", C_MUTED) + ACTION_GAP;
  ui_keycap(col, row + ACTIONS, "del", "Erase", C_MUTED);
  gfx_BlitRectangle(gfx_buffer, (uint24_t)x, (uint8_t)y, TEXT_W, FIELD + 2 * ACTIONS);
}

bool ui_edit(const char *title, const char *hint, char *text, size_t size)
{
  char work[WORK_SIZE];
  bool letters = true;
  size_t n;
  if (size > sizeof(work)) size = sizeof(work);
  strncpy(work, text, size - 1);
  work[size - 1] = 0;
  bar_y = 0;
  int y = panel(TONE_ACCENT, title, hint, FIELD + 2 * ACTIONS);
  ui_show();
  for (;;) {
    n = strlen(work);
    draw_input(y, work, letters);
    uint8_t key = ui_key();
    if (key == sk_Enter) {
      strcpy(text, work);
      return true;
    }
    if (key == sk_Clear) {
      if (!n) return false;
      work[0] = 0;
    } else if (key == sk_Alpha) letters = !letters;
    else if (key == sk_Del) {
      if (n) work[n - 1] = 0;
    } else {
      char c = key_char(key, letters);
      if (c && n + 1 < size) {
        work[n] = c;
        work[n + 1] = 0;
      }
    }
  }
}
