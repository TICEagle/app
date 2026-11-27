#include <string.h>
#include <ti/getcsc.h>

#include "app.h"
#include "http.h"
#include "store.h"
#include "text.h"
#include "ui.h"

#define LIST_Y 24
#define ROW_H 34
#define NAME_X 12
#define SWITCH_X 284
#define INFO_Y 104
#define KILOBYTE 1024ul
#define LINE_SIZE 48

static uint8_t selected;

static void draw_stat(int y, const char *label, const char *pattern, uint32_t value)
{
  char line[LINE_SIZE];
  text_format(line, LINE_SIZE, pattern, value);
  ui_text(NAME_X, y, C_MUTED, label, UI_WIDTH);
  ui_text(UI_WIDTH - UI_PAD - ui_width(line), y, C_FG, line, UI_WIDTH);
}

static void draw(void)
{
  static const char *const labels[5] = {"Change", NULL, NULL, NULL, "Back"};
  int edge = UI_WIDTH - UI_PAD - NAME_X;
  ui_screen("Settings", NULL);
  ui_row(LIST_Y, ROW_H, !selected);
  ui_bold(NAME_X, LIST_Y + 6, C_FG, "Server address", edge);
  ui_text(NAME_X, LIST_Y + 18, C_ACCENT, settings.server, edge);
  ui_row(LIST_Y + ROW_H, ROW_H, selected);
  ui_bold(NAME_X, LIST_Y + ROW_H + 6, C_FG, "Show pre-releases", edge);
  ui_text(NAME_X, LIST_Y + ROW_H + 18, C_MUTED, "Test versions that authors publish early", SWITCH_X - NAME_X - UI_PAD);
  ui_switch(SWITCH_X, LIST_Y + ROW_H + 11, settings.prereleases);
  ui_text(UI_PAD, INFO_Y, C_MUTED, "THIS CALCULATOR", UI_WIDTH);
  draw_stat(INFO_Y + UI_LINE + 4, "Free RAM", "%lu KB", store_free_ram() / KILOBYTE);
  draw_stat(INFO_Y + 2 * UI_LINE + 4, "Free archive", "%lu KB", store_free_archive() / KILOBYTE);
  draw_stat(INFO_Y + 3 * UI_LINE + 4, "Installed projects", "%lu", db.project_count);
  draw_stat(INFO_Y + 4 * UI_LINE + 4, "CEagle version", APP_VERSION, 0);
  ui_wrap(UI_PAD, INFO_Y + 6 * UI_LINE, UI_WIDTH - 2 * UI_PAD, 3, C_MUTED,
          "The server address starts with http:// and has no path, like http://ceagle.cc or "
          "http://192.168.1.2:8080.",
          false);
  ui_footer(labels);
}

static bool address_ok(const char *text)
{
  char host[HTTP_HOST_SIZE];
  uint16_t port;
  return http_server(text, host, &port);
}

uint8_t settings_run(void)
{
  selected = 0;
  for (;;) {
    ui_backdrop(draw);
    draw();
    ui_show();
    switch (ui_key()) {
    case sk_Up:
    case sk_Down: selected ^= 1; break;
    case sk_Enter:
    case sk_2nd:
    case sk_Yequ:
      if (selected) settings.prereleases = !settings.prereleases;
      else {
        char text[SERVER_SIZE];
        strcpy(text, settings.server);
        if (!ui_edit("Server address", "Like http://ceagle.cc", text, sizeof(text))) break;
        size_t n = strlen(text);
        while (n && text[n - 1] == '/') text[--n] = 0;
        if (!address_ok(text)) {
          ui_message(TONE_BAD, "Settings", "That is not a server address. It must look like http://ceagle.cc.");
          break;
        }
        strcpy(settings.server, text);
      }
      if (!store_save_settings()) ui_message(TONE_WARN, "Settings", "Not enough free RAM to save the settings.");
      ws_holds = WS_EMPTY;
      break;
    case sk_Clear:
    case sk_Graph:
    case sk_Mode: return SCREEN_BROWSE;
    }
  }
}
