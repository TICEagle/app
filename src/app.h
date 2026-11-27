#ifndef CEAGLE_APP_H
#define CEAGLE_APP_H

#include "api.h"
#include "db.h"

enum { SCREEN_BROWSE, SCREEN_INSTALLED, SCREEN_SETTINGS, SCREEN_QUIT };
enum { WS_EMPTY, WS_LIST, WS_UPDATES, WS_PLAN };

typedef union {
  project_list_t list;
  updates_t updates;
  plan_t plan;
} workspace_t;

extern db_t db;
extern json_t json;
extern workspace_t ws;
extern uint8_t ws_holds;
extern detail_t detail;

uint8_t browse_run(void);
void detail_run(const char *slug);
uint8_t installed_run(void);
void installed_takedowns(void);
uint8_t settings_run(void);
bool install_project(const char *slug, bool chosen);
bool uninstall_project(uint8_t project, bool ask);
void show_net_error(const char *title, uint8_t result);
const char *cap_label(uint8_t bit);
bool review_plan(const plan_t *plan);
#ifdef CEAGLE_TEST
void test_log(const char *tag);
#endif

#endif
