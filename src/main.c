#ifdef CEAGLE_TEST
#include <fileioc.h>
#include <lwip/core/mem.h>
#endif

#include "app.h"
#include "net.h"
#include "store.h"
#include "ui.h"

#ifdef CEAGLE_TEST
#include "text.h"
#endif

db_t db;
workspace_t ws;
uint8_t ws_holds;

#ifdef CEAGLE_TEST
void test_log(const char *tag)
{
  struct mem_accounting_stats stats = {0};
  char line[160];
  uint8_t h = ti_Open("CEATEST", "a");
  mem_get_stats(&stats);
  size_t n = text_format(line, sizeof(line), "%s ram=%lu archive=%lu heap_limit=%u heap_used=%u heap_free=%u\n", tag,
                         (unsigned long)store_free_ram(), (unsigned long)store_free_archive(), stats.heap_limit,
                         stats.heap_used, stats.heap_free);
  if (h) {
    ti_Write(line, n < sizeof(line) ? n : sizeof(line) - 1, 1, h);
    ti_Close(h);
  }
}
#endif

static void cleaning_up(void) { ui_busy("Archive", "Cleaning up archive memory..."); }

int main(void)
{
  uint8_t screen = SCREEN_BROWSE;
  ui_begin();
  store_load_settings();
  uint8_t loaded = store_load_db(&db);
  ui_busy("Network", "Starting the network...");
  if (!net_start()) {
    ui_message(TONE_BAD, "lwIP-CE missing",
               "CEagle needs lwIP-CE 3.0. Run its installer, prgmINSTALL, then start CEagle again.");
    net_stop();
    ui_end();
    return 0;
  }
  store_gc_hooks(cleaning_up);
#ifdef CEAGLE_TEST
  test_log("start");
#endif
  if (loaded == STORE_DB_DAMAGED)
    ui_message(TONE_WARN, "Installed list damaged",
               "The list of installed programs (CEAGLEDB) is damaged, so CEagle starts with an empty list.");
  installed_takedowns();
  while (screen != SCREEN_QUIT)
    if (screen == SCREEN_BROWSE) screen = browse_run();
    else if (screen == SCREEN_INSTALLED) screen = installed_run();
    else screen = settings_run();
#ifdef CEAGLE_TEST
  test_log("quit");
#endif
  net_stop();
  ui_end();
  return 0;
}
