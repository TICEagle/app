#ifndef CEAGLE_UI_H
#define CEAGLE_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_WIDTH 320
#define UI_HEIGHT 240
#define UI_HEADER 20
#define UI_FOOTER 220
#define UI_PAD 8
#define UI_LINE 12

enum {
  C_BG,
  C_SURFACE,
  C_LINE,
  C_MUTED,
  C_FG,
  C_BAR,
  C_ON_BAR,
  C_SELECT,
  C_ACCENT,
  C_OK,
  C_WARN,
  C_BAD,
  C_INFO,
  C_ACCENT_TINT,
  C_OK_TINT,
  C_WARN_TINT,
  C_BAD_TINT,
  C_INFO_TINT,
  C_ON_ACCENT,
  C_COUNT
};

enum { TONE_ACCENT, TONE_OK, TONE_WARN, TONE_BAD, TONE_INFO };

void ui_begin(void);
void ui_end(void);
void ui_backdrop(void (*draw)(void));
void ui_screen(const char *title, const char *right);
void ui_footer(const char *const labels[5]);
void ui_fill(int x, int y, int w, int h, uint8_t color);
int ui_width(const char *text);
int ui_text(int x, int y, uint8_t color, const char *text, int max);
int ui_bold(int x, int y, uint8_t color, const char *text, int max);
uint8_t ui_wrap(int x, int y, int w, uint8_t max_lines, uint8_t color, const char *text, bool center);
int ui_title(int x, int y, const char *text, int max);
void ui_switch(int x, int y, bool on);
int ui_chip(int x, int y, uint8_t tone, bool filled, const char *text);
int ui_keycap(int x, int y, const char *key, const char *label, uint8_t color);
void ui_row(int y, int h, bool selected);
void ui_scrollbar(int y, int h, uint8_t top, uint8_t visible, uint8_t total);
void ui_empty(int y, const char *title, const char *text);
void ui_show(void);
uint8_t ui_key(void);
void ui_dim(void);
void ui_message(uint8_t tone, const char *title, const char *text);
bool ui_confirm(uint8_t tone, const char *title, const char *text, const char *yes);
void ui_busy(const char *title, const char *text);
void ui_progress(uint32_t done, uint32_t total);
bool ui_edit(const char *title, const char *hint, char *text, size_t size);

#endif
