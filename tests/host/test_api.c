#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "check.h"

static bool run(json_handler_t handler, void *arg, const char *text)
{
  json_t json;
  json_init(&json, handler, arg);
  return json_feed(&json, (const uint8_t *)text, strlen(text)) && json_finish(&json);
}

static const char *const sha_a = "a3b1c2d4e5f60718293a4b5c6d7e8f90a1b2c3d4e5f60718293a4b5c6d7e8f90";

static char *install_doc(int files, const char *extra)
{
  char *doc = malloc(1024 + (size_t)files * 600);
  size_t n =
      (size_t)sprintf(doc, "{\"slug\":\"gdash\",\"version\":\"1.2.0\",\"capabilities\":[\"delete_vars\",\"new_thing\"],"
                           "\"provenance\":{\"via\":\"session\"},\"files\":[");
  for (int i = 0; i < files; i++)
    n += (size_t)sprintf(doc + n,
                         "%s{\"slug\":\"%s\",\"version\":\"%s\",\"name\":\"F%d\",\"name_hex\":\"46%02X\",\"type\":%d,"
                         "\"archived\":%s,\"size\":100,\"sha256\":\"%s\",\"url\":\"/api/v1/dl/x/1/F.8xp\","
                         "\"raw_url\":\"/api/v1/dl/%s/%s/F%d.8xp?format=raw\",\"raw_size\":%d,\"raw_sha256\":\"%s\"%s}",
                         i ? "," : "", i == files - 1 ? "gdash" : "graphx", i == files - 1 ? "1.2.0" : "12.0.0", i,
                         'A' + i % 26, i == files - 1 ? 6 : 21, i % 2 ? "true" : "false", sha_a,
                         i == files - 1 ? "gdash" : "graphx", i == files - 1 ? "1.2.0" : "12.0.0", i, 40 + i, sha_a,
                         i == files - 1 ? extra : "");
  sprintf(doc + n, "]}");
  return doc;
}

static void list_and_detail(void)
{
  project_list_t list = {0};
  CHECK(run(
      api_list, &list,
      "{\"projects\":[{\"slug\":\"gdash\",\"name\":\"Geometry Dash CE with an extra long name here\",\"kind\":\"asm\","
      "\"summary\":\"A rhythm "
      "game.\",\"latest_version\":\"1.2.0\",\"owner\":\"bob\",\"created_at\":\"2026\"},{\"slug\":\"lib-x\",\"name\":"
      "\"L\","
      "\"kind\":\"library\",\"summary\":\"\"}],\"page\":1,\"more\":true}"));
  CHECK(list.count == 2 && list.more);
  CHECK(!strcmp(list.items[0].slug, "gdash") && list.items[0].kind == KIND_ASM);
  CHECK(strlen(list.items[0].name) == LIST_NAME_SIZE - 1);
  CHECK(!strcmp(list.items[0].latest, "1.2.0") && !list.items[1].latest[0]);
  CHECK(list.items[1].kind == KIND_LIBRARY);
  project_list_t bad = {0};
  CHECK(!run(api_list, &bad, "{\"projects\":[{\"slug\":\"BAD SLUG\"}]}"));
  project_list_t odd = {0};
  CHECK(run(api_list, &odd, "{\"projects\":[[],[1],\"x\",{\"slug\":\"ok\"},[]],\"more\":[true]}") && odd.count == 1);
  static plan_t odd_plan;
  CHECK(!run(api_install, &odd_plan, "{\"files\":[[]]}") && odd_plan.error == PLAN_E_INVALID);
  char many[8192];
  size_t n = (size_t)sprintf(many, "{\"projects\":[");
  for (int i = 0; i < LIST_MAX + 1; i++) n += (size_t)sprintf(many + n, "%s{\"slug\":\"p%d\"}", i ? "," : "", i + 10);
  sprintf(many + n, "]}");
  project_list_t over = {0};
  CHECK(!run(api_list, &over, many));
  detail_t d = {0};
  CHECK(run(
      api_detail, &d,
      "{\"slug\":\"gdash\",\"name\":\"Geometry "
      "Dash\",\"kind\":\"asm\",\"parent\":null,\"owner\":\"bob\",\"summary\":\"Jump.\","
      "\"description\":\"# Long\",\"latest\":\"1.2.0\",\"capabilities\":[\"usb\",\"link\"],\"taken_down_at\":null}"));
  CHECK(!strcmp(d.latest, "1.2.0") && d.caps == (CAP_USB | CAP_LINK) && !d.taken_down && d.kind == KIND_ASM);
  detail_t gone = {0};
  CHECK(run(api_detail, &gone,
            "{\"slug\":\"gdash\",\"latest\":null,\"capabilities\":[],\"taken_down_at\":\"2026-11-26\"}"));
  CHECK(gone.taken_down && !gone.latest[0]);
  detail_t longver = {0};
  CHECK(!run(api_detail, &longver, "{\"slug\":\"gdash\",\"latest\":\"1.0.0-a-very-long-prerelease-tag\"}"));
  char message[MESSAGE_SIZE] = "";
  CHECK(run(api_error, message, "{\"error\":\"there is no project named x\"}"));
  CHECK(!strcmp(message, "there is no project named x"));
}

static void install(void)
{
  static plan_t plan;
  char *doc = install_doc(3, ",\"main\":true");
  memset(&plan, 0, sizeof(plan));
  CHECK(run(api_install, &plan, doc) && api_install_done(&plan));
  CHECK(plan.file_count == 3 && plan.project_count == 2);
  CHECK(!strcmp(plan.root, "gdash") && plan.caps == (CAP_DELETE_VARS | CAP_OTHER));
  CHECK(!strcmp(plan.projects[0].slug, "graphx") && !strcmp(plan.projects[1].version, "1.2.0"));
  CHECK(plan.files[2].project == 1 && plan.files[2].type == 6 && plan.files[2].raw_size == 42);
  CHECK(plan.files[1].flags == FILE_ARCHIVED && plan.files[0].flags == 0);
  CHECK(!strcmp(plan.pool + plan.files[2].filename, "F2.8xp"));
  CHECK(plan.files[0].name[0] == 'F' && plan.files[0].name[1] == 'A' && plan.files[0].name[2] == 0);
  free(doc);

  doc = install_doc(PLAN_MAX_FILES, "");
  memset(&plan, 0, sizeof(plan));
  CHECK(!run(api_install, &plan, doc));
  CHECK(plan.error == PLAN_E_DUPLICATE);
  free(doc);

  static const char *const broken[] = {"\"raw_size\":70000",
                                       "\"raw_sha256\":\"abc\"",
                                       "\"name_hex\":\"zz\"",
                                       "\"type\":8",
                                       "\"raw_url\":\"http://evil/x?format=raw\"",
                                       "\"raw_url\":\"/api/v1/dl/gdash/1.2.0/../x?format=raw\"",
                                       "\"type\":-1",
                                       "\"raw_size\":1",
                                       "\"version\":\"9.9.9\""};
  static const uint8_t errors[] = {PLAN_E_TOO_LARGE, PLAN_E_INVALID, PLAN_E_INVALID, PLAN_E_INVALID, PLAN_E_INVALID,
                                   PLAN_E_INVALID,   PLAN_E_INVALID, PLAN_E_INVALID, PLAN_E_INVALID};
  for (size_t i = 0; i < sizeof(broken) / sizeof(broken[0]); i++) {
    char extra[200];
    snprintf(extra, sizeof(extra), ",%s", broken[i]);
    doc = install_doc(2, extra);
    memset(&plan, 0, sizeof(plan));
    bool ok = run(api_install, &plan, doc) && api_install_done(&plan);
    if (ok || plan.error != errors[i]) fprintf(stderr, "case %zu: ok=%d error=%u\n", i, ok, plan.error);
    CHECK(!ok && plan.error == errors[i]);
    free(doc);
  }
  memset(&plan, 0, sizeof(plan));
  CHECK(run(api_install, &plan, "{\"slug\":\"gdash\",\"version\":\"1.0.0\",\"files\":[]}") && !api_install_done(&plan));
  memset(&plan, 0, sizeof(plan));
  CHECK(
      !run(api_install, &plan, "{\"slug\":\"gdash\",\"version\":\"1.0.0-this-is-way-too-long-for-us\",\"files\":[]}"));
  CHECK(plan.error == PLAN_E_VERSION);
}

static char *tree_doc(int projects, int files_each, int name_length)
{
  size_t n = 0;
  char *doc = malloc((size_t)projects * files_each * 900 + 1024);
  n += (size_t)sprintf(doc, "{\"slug\":\"p00\",\"version\":\"1.0.0\",\"capabilities\":[],\"files\":[");
  for (int p = 0; p < projects; p++)
    for (int f = 0; f < files_each; f++) {
      int i = p * files_each + f;
      n += (size_t)sprintf(doc + n,
                           "%s{\"slug\":\"p%02d\",\"version\":\"1.0.0\",\"name\":\"X\",\"name_hex\":\"%02X%02X%02X\","
                           "\"type\":5,\"archived\":true,\"size\":1,\"sha256\":\"%s\",\"url\":\"u\",\"raw_url\":"
                           "\"/api/v1/dl/p%02d/1.0.0/%0*d?format=raw\",\"raw_size\":65506,\"raw_sha256\":\"%s\"}",
                           i ? "," : "", p, 'A' + i / 676 % 26, 'A' + i / 26 % 26, 'A' + i % 26, sha_a, p, name_length,
                           i, sha_a);
    }
  sprintf(doc + n, "]}");
  return doc;
}

static void max_install(void)
{
  static plan_t plan;
  char *doc = tree_doc(64, 64, 64);
  memset(&plan, 0, sizeof(plan));
  CHECK(!run(api_install, &plan, doc) && plan.error == PLAN_E_TOO_MANY_FILES);
  free(doc);
  doc = tree_doc(64, 1, 64);
  memset(&plan, 0, sizeof(plan));
  CHECK(run(api_install, &plan, doc) && api_install_done(&plan));
  CHECK(plan.project_count == 64 && plan.file_count == 64);
  free(doc);
  doc = tree_doc(65, 1, 8);
  memset(&plan, 0, sizeof(plan));
  CHECK(!run(api_install, &plan, doc) && plan.error == PLAN_E_TOO_MANY_PROJECTS);
  free(doc);
  doc = tree_doc(1, 64, 64);
  memset(&plan, 0, sizeof(plan));
  CHECK(run(api_install, &plan, doc) && api_install_done(&plan) && plan.file_count == 64);
  free(doc);
  doc = tree_doc(2, 48, FILENAME_MAX_LENGTH);
  memset(&plan, 0, sizeof(plan));
  CHECK(run(api_install, &plan, doc) && api_install_done(&plan) && plan.file_count == PLAN_MAX_FILES);
  free(doc);
}

static void updates(void)
{
  static db_t db;
  static updates_t u;
  memset(&db, 0, sizeof(db));
  memset(&u, 0, sizeof(u));
  db_add(&db, "gdash");
  db_add(&db, "old");
  db_add(&db, "gone");
  u.db = &db;
  CHECK(
      run(api_updates, &u,
          "{\"updates\":[{\"slug\":\"gdash\",\"version\":\"1.3.0\",\"capabilities\":[\"network\"]},{\"slug\":\"gone\","
          "\"taken_down\":true},{\"slug\":\"old\"},{\"slug\":\"nobody\",\"version\":\"1.0.0\"}]}"));
  CHECK(u.items[0].state == UPDATE_LATEST && !strcmp(u.items[0].latest, "1.3.0") && u.items[0].caps == CAP_NETWORK);
  CHECK(u.items[1].state == UPDATE_UNKNOWN && u.items[2].state == UPDATE_TAKEN_DOWN);
}

static void versions(void)
{
  static const char *const order[] = {"0.9.0",      "1.0.0-alpha",  "1.0.0-alpha.1", "1.0.0-alpha.beta",
                                      "1.0.0-beta", "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1",
                                      "1.0.0",      "1.0.10",       "1.2.0",         "10.0.0"};
  size_t n = sizeof(order) / sizeof(order[0]);
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < n; j++) {
      int c = version_compare(order[i], order[j]);
      CHECK((i < j && c < 0) || (i == j && c == 0) || (i > j && c > 0));
    }
}

static void hostile(void)
{
  static plan_t plan;
  static db_t db;
  static updates_t u;
  static project_list_t list;
  static detail_t d;
  static const json_handler_t handlers[] = {api_install, api_list, api_detail, api_updates};
  void *args[] = {&plan, &list, &d, &u};
  char *doc = install_doc(4, ",\"main\":true");
  size_t n = strlen(doc);
  char *copy = malloc(n + 1);
  db_add(&db, "gdash");
  db_add(&db, "graphx");
  for (int round = 0; round < 40000; round++) {
    memcpy(copy, doc, n + 1);
    for (int k = rng() % 4; k >= 0; k--) {
      size_t at = rng() % n;
      copy[at] = "{}[],:\"0aZ\\"[rng() % 12];
    }
    for (size_t h = 0; h < sizeof(handlers) / sizeof(handlers[0]); h++) {
      json_t json;
      memset(&plan, 0, sizeof(plan));
      memset(&list, 0, sizeof(list));
      memset(&d, 0, sizeof(d));
      memset(&u, 0, sizeof(u));
      u.db = &db;
      json_init(&json, handlers[h], args[h]);
      if (json_feed(&json, (const uint8_t *)copy, n) && json_finish(&json) && h == 0 && api_install_done(&plan))
        for (uint8_t i = 0; i < plan.file_count; i++) CHECK(plan.files[i].project < plan.project_count);
    }
  }
  free(copy);
  free(doc);
}

static void calculator_view(void)
{
  project_list_t list = {0};
  CHECK(run(api_list, &list,
            "{\"projects\":[{\"slug\":\"solver\",\"name\":\"Solver\",\"kind\":\"asm\",\"summary\":\"Solves.\","
            "\"latest_version\":null},{\"slug\":\"pong\",\"name\":\"Pong\",\"kind\":\"basic\",\"summary\":\"\","
            "\"latest_version\":\"2.0.0\"}],\"more\":false}"));
  CHECK(list.count == 2 && !list.more && list.items[0].kind == KIND_ASM && !list.items[0].latest[0]);
  CHECK(!strcmp(list.items[1].latest, "2.0.0") && !strcmp(list.items[0].summary, "Solves."));
  detail_t d = {0};
  CHECK(run(api_detail, &d,
            "{\"slug\":\"solver\",\"name\":\"Solver\",\"kind\":\"asm\",\"owner\":\"alice\",\"summary\":\"Solves.\","
            "\"latest\":null,\"capabilities\":null,\"taken_down_at\":null}"));
  CHECK(!strcmp(d.owner, "alice") && !d.latest[0] && !d.caps && !d.taken_down && d.kind == KIND_ASM);

  char path[192];
  api_list_path(path, sizeof(path), 2, "", "", "");
  CHECK(!strcmp(path, "/api/v1/projects?view=calculator&page=2&kind=&category=&q="));
  api_list_path(path, sizeof(path), 1, "basic", "math", "geo dash:2");
  CHECK(!strcmp(path, "/api/v1/projects?view=calculator&page=1&kind=basic&category=math&q=geo%20dash%3A2"));
  char query[33];
  memset(query, ' ', 32);
  query[32] = 0;
  api_list_path(path, sizeof(path), 65535, "library", "programming", query);
  CHECK(strlen(path) == 80 + 96);
  CHECK(!strcmp(path + strlen(path) - 6, "%20%20"));
  char small[70];
  api_list_path(small, sizeof(small), 1, "library", "programming", query);
  CHECK(strlen(small) < sizeof(small) && !strncmp(small, "/api/v1/projects?view=calculator", 32));
  char tiny[8];
  api_list_path(tiny, sizeof(tiny), 1, "", "", "abc");
  CHECK(strlen(tiny) == sizeof(tiny) - 1);
}

void test_api(void)
{
  hostile();
  uint8_t out[4], len;
  CHECK(hex_decode("0aFf", out, 4, &len) && len == 2 && out[1] == 0xFF);
  CHECK(!hex_decode("abc", out, 4, &len));
  CHECK(!hex_decode("0102030405", out, 4, &len));
  list_and_detail();
  calculator_view();
  install();
  max_install();
  updates();
  versions();
}
