#include <graphx.h>
#include <string.h>
#include <ti/getcsc.h>

#include "net.h"
#include "ui.h"

#define TRANSPARENT 0xFF
#define IDLE_MARK 0x07
#define LINE_MAX 80
#define ELLIPSIS "..."
#define BRAND "CEagle"
#define BRAND_GAP 10
#define TEXT_DROP 6
#define CHIP_PAD 4
#define CHIP_HEIGHT 12
#define SLOT_WIDTH 64
#define SLOT_DIVIDER 10
#define SCROLL_WIDTH 3
#define SCROLL_MIN 10
#define EMPTY_MARGIN 40
#define TITLE_SCALE 2
#define SWITCH_W 24

static const uint16_t palette[C_COUNT] = {
    gfx_RGBTo1555(12, 16, 19),    gfx_RGBTo1555(22, 28, 32),    gfx_RGBTo1555(44, 51, 56),
    gfx_RGBTo1555(152, 160, 165), gfx_RGBTo1555(228, 232, 235), gfx_RGBTo1555(22, 38, 49),
    gfx_RGBTo1555(239, 242, 245), gfx_RGBTo1555(24, 46, 52),    gfx_RGBTo1555(78, 204, 211),
    gfx_RGBTo1555(118, 207, 138), gfx_RGBTo1555(243, 189, 92),  gfx_RGBTo1555(251, 129, 122),
    gfx_RGBTo1555(146, 185, 248), gfx_RGBTo1555(30, 57, 61),    gfx_RGBTo1555(38, 58, 48),
    gfx_RGBTo1555(60, 55, 40),    gfx_RGBTo1555(62, 44, 45),    gfx_RGBTo1555(43, 54, 68),
    gfx_RGBTo1555(5, 18, 19),
};

static const uint8_t dimmed[C_COUNT] = {
    C_BG,   C_BG,   C_BG,   C_LINE,    C_LINE,    C_SURFACE, C_LINE,    C_SURFACE, C_LINE, C_LINE,
    C_LINE, C_LINE, C_LINE, C_SURFACE, C_SURFACE, C_SURFACE, C_SURFACE, C_SURFACE, C_BG,
};

static void (*backdrop)(void);

void ui_begin(void)
{
  gfx_Begin();
  gfx_SetPalette(palette, sizeof(palette), 0);
  gfx_SetDrawBuffer();
  gfx_SetMonospaceFont(0);
  gfx_SetTextTransparentColor(TRANSPARENT);
  gfx_SetTextBGColor(TRANSPARENT);
}

void ui_end(void) { gfx_End(); }

void ui_backdrop(void (*draw)(void)) { backdrop = draw; }

void ui_fill(int x, int y, int w, int h, uint8_t color)
{
  if (w <= 0 || h <= 0) return;
  gfx_SetColor(color);
  gfx_FillRectangle_NoClip((uint24_t)x, (uint8_t)y, (uint24_t)w, (uint8_t)h);
}

static char clean(char c) { return c >= 0x20 && c < 0x7F ? c : '?'; }

int ui_width(const char *text)
{
  int w = 0;
  for (; *text; text++) w += (int)gfx_GetCharWidth(clean(*text));
  return w;
}

static int fit(char *line, const char *text, int max)
{
  int w = 0, ellipsis = ui_width(ELLIPSIS);
  size_t n = 0;
  while (text[n] && n < LINE_MAX) {
    line[n] = clean(text[n]);
    w += (int)gfx_GetCharWidth(line[n]);
    n++;
  }
  line[n] = 0;
  if (w <= max) return w;
  while (n && w + ellipsis > max) w -= (int)gfx_GetCharWidth(line[--n]);
  strcpy(line + n, ELLIPSIS);
  return w + ellipsis;
}

int ui_text(int x, int y, uint8_t color, const char *text, int max)
{
  char line[LINE_MAX + sizeof(ELLIPSIS)];
  int w = fit(line, text, max);
  gfx_SetTextFGColor(color);
  gfx_PrintStringXY(line, x, y);
  return w;
}

int ui_bold(int x, int y, uint8_t color, const char *text, int max)
{
  ui_text(x + 1, y, color, text, max - 1);
  return ui_text(x, y, color, text, max - 1) + 1;
}

static size_t line_break(const char *text, int w)
{
  size_t n = 0, space = 0;
  int used = 0;
  while (text[n]) {
    used += (int)gfx_GetCharWidth(clean(text[n]));
    if (used > w) return space ? space : (n ? n : 1);
    if (text[n] == ' ') space = n;
    n++;
  }
  return n;
}

uint8_t ui_wrap(int x, int y, int w, uint8_t max_lines, uint8_t color, const char *text, bool center)
{
  uint8_t lines = 0;
  while (*text == ' ') text++;
  while (*text && lines < max_lines) {
    char line[LINE_MAX + 1];
    size_t n = line_break(text, w);
    bool last = lines + 1 == max_lines && text[n];
    if (n > LINE_MAX) n = LINE_MAX;
    memcpy(line, text, n);
    line[n] = 0;
    if (y >= 0) {
      const char *shown = last ? text : line;
      int lx = center ? x + (w - (last ? w : ui_width(line))) / 2 : x;
      ui_text(lx, y + lines * UI_LINE, color, shown, w);
    }
    lines++;
    text += n;
    while (*text == ' ') text++;
  }
  return lines;
}

int ui_chip(int x, int y, uint8_t tone, bool filled, const char *text)
{
  int w = ui_width(text) + 2 * CHIP_PAD;
  ui_fill(x, y, w, CHIP_HEIGHT, filled ? C_ACCENT + tone : C_ACCENT_TINT + tone);
  ui_text(x + CHIP_PAD, y + 2, filled ? C_ON_ACCENT : C_ACCENT + tone, text, w);
  return w;
}

int ui_keycap(int x, int y, const char *key, const char *label, uint8_t color)
{
  int w = ui_width(key) + 2 * CHIP_PAD;
  ui_fill(x, y, w, CHIP_HEIGHT, C_LINE);
  ui_text(x + CHIP_PAD, y + 2, C_FG, key, w);
  w += CHIP_PAD;
  return w + ui_bold(x + w, y + 2, color, label, UI_WIDTH);
}

int ui_title(int x, int y, const char *text, int max)
{
  char line[LINE_MAX + sizeof(ELLIPSIS)];
  if (ui_width(text) * TITLE_SCALE > max) {
    ui_bold(x, y, C_FG, text, max);
    return UI_LINE;
  }
  fit(line, text, max);
  gfx_SetTextScale(TITLE_SCALE, TITLE_SCALE);
  gfx_SetTextFGColor(C_FG);
  gfx_PrintStringXY(line, x, y);
  gfx_SetTextScale(1, 1);
  return UI_LINE * TITLE_SCALE;
}

void ui_switch(int x, int y, bool on)
{
  ui_fill(x, y, SWITCH_W, CHIP_HEIGHT, on ? C_ACCENT : C_LINE);
  ui_fill(on ? x + SWITCH_W - CHIP_HEIGHT + 2 : x + 2, y + 2, CHIP_HEIGHT - 4, CHIP_HEIGHT - 4,
          on ? C_ON_ACCENT : C_MUTED);
}

void ui_screen(const char *title, const char *right)
{
  int x = UI_PAD;
  ui_fill(0, 0, UI_WIDTH, UI_HEIGHT, C_BG);
  ui_fill(0, 0, UI_WIDTH, UI_HEADER, C_BAR);
  ui_fill(0, UI_HEADER - 1, UI_WIDTH, 1, C_LINE);
  x += ui_bold(x, TEXT_DROP, C_ACCENT, BRAND, UI_WIDTH) + BRAND_GAP;
  int right_w = right ? ui_width(right) : 0;
  if (right) ui_text(UI_WIDTH - UI_PAD - right_w, TEXT_DROP, C_MUTED, right, right_w);
  if (title) ui_text(x, TEXT_DROP, C_ON_BAR, title, UI_WIDTH - x - right_w - 2 * UI_PAD);
}

void ui_footer(const char *const labels[5])
{
  ui_fill(0, UI_FOOTER, UI_WIDTH, UI_HEIGHT - UI_FOOTER, C_BAR);
  ui_fill(0, UI_FOOTER, UI_WIDTH, 1, C_LINE);
  for (int i = 0; i < 5; i++) {
    if (i) ui_fill(i * SLOT_WIDTH, UI_FOOTER + 5, 1, SLOT_DIVIDER, C_LINE);
    if (!labels[i]) continue;
    int w = ui_width(labels[i]);
    ui_text(i * SLOT_WIDTH + (SLOT_WIDTH - w) / 2, UI_FOOTER + 7, C_ON_BAR, labels[i], SLOT_WIDTH);
  }
}

void ui_row(int y, int h, bool selected)
{
  if (selected) {
    ui_fill(0, y, UI_WIDTH, h, C_SELECT);
    ui_fill(0, y, SCROLL_WIDTH, h, C_ACCENT);
  } else ui_fill(UI_PAD, y + h - 1, UI_WIDTH - 2 * UI_PAD, 1, C_SURFACE);
}

void ui_scrollbar(int y, int h, uint8_t top, uint8_t visible, uint8_t total)
{
  if (total <= visible) return;
  int thumb = h * visible / total;
  if (thumb < SCROLL_MIN) thumb = SCROLL_MIN;
  ui_fill(UI_WIDTH - SCROLL_WIDTH, y, SCROLL_WIDTH, h, C_SURFACE);
  ui_fill(UI_WIDTH - SCROLL_WIDTH, y + (h - thumb) * top / (total - visible), SCROLL_WIDTH, thumb, C_MUTED);
}

void ui_empty(int y, const char *title, const char *text)
{
  ui_bold((UI_WIDTH - ui_width(title)) / 2, y, C_FG, title, UI_WIDTH);
  ui_wrap(EMPTY_MARGIN, y + UI_LINE + 6, UI_WIDTH - 2 * EMPTY_MARGIN, 4, C_MUTED, text, true);
}

void ui_dim(void)
{
  uint8_t *pixel = (uint8_t *)gfx_vbuffer;
  if (backdrop) backdrop();
  else ui_screen(NULL, NULL);
  for (uint24_t i = 0; i < (uint24_t)UI_WIDTH * UI_HEIGHT; i++) pixel[i] = pixel[i] < C_COUNT ? dimmed[pixel[i]] : C_BG;
}

void ui_show(void) { gfx_BlitBuffer(); }

uint8_t ui_key(void)
{
  uint8_t key;
#ifdef CEAGLE_TEST
  gfx_vram[0] = IDLE_MARK;
#endif
  while (!(key = os_GetCSC())) net_idle();
  return key;
}
