#include <string.h>

#include "api.h"

#define SEEN_SLUG 1
#define SEEN_VERSION 2
#define SEEN_NAME 4
#define SEEN_TYPE 8
#define SEEN_ARCHIVED 16
#define SEEN_URL 32
#define SEEN_SIZE 64
#define SEEN_SHA 128
#define SEEN_ALL 255
#define DOWNLOAD_PREFIX "/api/v1/dl/"
#define RAW_SUFFIX "?format=raw"
#define TYPE_MAX 0xFF

static const char *const cap_names[] = {"flash_write", "delete_vars", "ram_reset", "usb",
                                        "network",     "inline_asm",  "link"};
static const char *const kind_names[] = {"basic", "asm", "library", "appvar"};

uint8_t cap_bit(const char *name)
{
  for (uint8_t i = 0; i < sizeof(cap_names) / sizeof(cap_names[0]); i++)
    if (!strcmp(name, cap_names[i])) return (uint8_t)(1 << i);
  return CAP_OTHER;
}

uint8_t kind_parse(const char *name)
{
  for (uint8_t i = 0; i < sizeof(kind_names) / sizeof(kind_names[0]); i++)
    if (!strcmp(name, kind_names[i])) return (uint8_t)(i + 1);
  return KIND_UNKNOWN;
}

static bool parse_uint(const char *s, uint32_t max, uint32_t *out)
{
  uint32_t v = 0;
  if (!*s) return false;
  for (; *s; s++) {
    if (*s < '0' || *s > '9') return false;
    v = v * 10 + (uint32_t)(*s - '0');
    if (v > max) return false;
  }
  *out = v;
  return true;
}

static int hex_value(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool hex_decode(const char *hex, uint8_t *out, uint8_t max, uint8_t *length)
{
  uint8_t n = 0;
  while (hex[0] && hex[1]) {
    int hi = hex_value(hex[0]), lo = hex_value(hex[1]);
    if (hi < 0 || lo < 0 || n == max) return false;
    out[n++] = (uint8_t)(hi << 4 | lo);
    hex += 2;
  }
  *length = n;
  return !hex[0];
}

static int compare_identifier(const char *a, size_t an, const char *b, size_t bn)
{
  bool a_num = an > 0, b_num = bn > 0;
  for (size_t i = 0; i < an; i++) a_num &= a[i] >= '0' && a[i] <= '9';
  for (size_t i = 0; i < bn; i++) b_num &= b[i] >= '0' && b[i] <= '9';
  if (a_num && b_num && an != bn) return an < bn ? -1 : 1;
  if (a_num != b_num) return a_num ? -1 : 1;
  int c = memcmp(a, b, an < bn ? an : bn);
  if (c) return c < 0 ? -1 : 1;
  return an == bn ? 0 : an < bn ? -1 : 1;
}

static size_t part_length(const char *s, size_t n)
{
  size_t i = 0;
  while (i < n && s[i] != '.') i++;
  return i;
}

static int compare_parts(const char *a, size_t an, const char *b, size_t bn)
{
  while (an && bn) {
    size_t x = part_length(a, an), y = part_length(b, bn);
    int c = compare_identifier(a, x, b, y);
    if (c) return c;
    a += x;
    an -= x;
    b += y;
    bn -= y;
    if (an) a++, an--;
    if (bn) b++, bn--;
  }
  return an ? 1 : bn ? -1 : 0;
}

int version_compare(const char *a, const char *b)
{
  const char *ad = strchr(a, '-'), *bd = strchr(b, '-');
  int c = compare_parts(a, ad ? (size_t)(ad - a) : strlen(a), b, bd ? (size_t)(bd - b) : strlen(b));
  if (c || (!ad && !bd)) return c;
  if (!ad || !bd) return ad ? -1 : 1;
  return compare_parts(ad + 1, strlen(ad + 1), bd + 1, strlen(bd + 1));
}

static bool copy_value(const json_t *json, char *dst, size_t size, bool exact)
{
  size_t n = strlen(json->value);
  if (exact && (json->truncated || n >= size)) return false;
  if (n >= size) n = size - 1;
  memcpy(dst, json->value, n);
  dst[n] = 0;
  return true;
}

bool api_list(json_t *json, uint8_t event)
{
  project_list_t *list = json->arg;
  if (json->depth == 1 && event == JSON_TRUE && json_key(json, 1, "more")) list->more = true;
  if (json->depth < 2 || !json_key(json, 1, "projects")) return true;
  if (json->depth == 2 && event == JSON_OBJECT) {
    if (list->count == LIST_MAX) return false;
    memset(&list->items[list->count++], 0, sizeof(list_item_t));
    return true;
  }
  if (json->depth == 2 && event == JSON_END) return !list->count || slug_valid(list->items[list->count - 1].slug);
  if (json->depth != 3 || event != JSON_STRING || !list->count) return true;
  list_item_t *item = &list->items[list->count - 1];
  if (json_key(json, 3, "slug")) return copy_value(json, item->slug, sizeof(item->slug), true);
  if (json_key(json, 3, "name")) return copy_value(json, item->name, sizeof(item->name), false);
  if (json_key(json, 3, "summary")) return copy_value(json, item->summary, sizeof(item->summary), false);
  if (json_key(json, 3, "latest_version")) copy_value(json, item->latest, sizeof(item->latest), true);
  if (json_key(json, 3, "kind")) item->kind = kind_parse(json->value);
  return true;
}

bool api_detail(json_t *json, uint8_t event)
{
  detail_t *d = json->arg;
  if (json->depth == 2 && event == JSON_STRING && json_key(json, 1, "capabilities")) d->caps |= cap_bit(json->value);
  if (json->depth != 1) return true;
  if (json_key(json, 1, "taken_down_at")) d->taken_down = event != JSON_NULL;
  if (event != JSON_STRING) return true;
  if (json_key(json, 1, "slug")) return copy_value(json, d->slug, sizeof(d->slug), true) && slug_valid(d->slug);
  if (json_key(json, 1, "name")) return copy_value(json, d->name, sizeof(d->name), false);
  if (json_key(json, 1, "owner")) return copy_value(json, d->owner, sizeof(d->owner), false);
  if (json_key(json, 1, "summary")) return copy_value(json, d->summary, sizeof(d->summary), false);
  if (json_key(json, 1, "latest")) return copy_value(json, d->latest, sizeof(d->latest), true);
  if (json_key(json, 1, "kind")) d->kind = kind_parse(json->value);
  return true;
}

static bool fail(plan_t *plan, uint8_t error)
{
  if (!plan->error) plan->error = error;
  return false;
}

static bool filename_char(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static bool take_filename(plan_t *plan)
{
  const char *url = plan->current_url;
  size_t prefix = strlen(DOWNLOAD_PREFIX), slug = strlen(plan->current_slug), version = strlen(plan->current_version);
  size_t suffix = strlen(RAW_SUFFIX), n = strlen(url);
  if (n <= prefix + slug + version + 2 + suffix || strncmp(url, DOWNLOAD_PREFIX, prefix) ||
      strncmp(url + prefix, plan->current_slug, slug) || url[prefix + slug] != '/' ||
      strncmp(url + prefix + slug + 1, plan->current_version, version) || url[prefix + slug + 1 + version] != '/' ||
      strcmp(url + n - suffix, RAW_SUFFIX))
    return false;
  const char *name = url + prefix + slug + version + 2;
  size_t length = (size_t)(url + n - suffix - name);
  if (length > FILENAME_MAX_LENGTH) return false;
  for (size_t i = 0; i < length; i++)
    if (!filename_char(name[i])) return false;
  if (plan->pool_used + length + 1 > PLAN_POOL_SIZE) return fail(plan, PLAN_E_TOO_MANY_FILES);
  plan->current.filename = plan->pool_used;
  memcpy(plan->pool + plan->pool_used, name, length);
  plan->pool[plan->pool_used + length] = 0;
  plan->pool_used = (uint16_t)(plan->pool_used + length + 1);
  return true;
}

static bool file_field(plan_t *plan, json_t *json, uint8_t event)
{
  plan_file_t *f = &plan->current;
  uint32_t number;
  if (json_key(json, 3, "archived") && (event == JSON_TRUE || event == JSON_FALSE)) {
    f->flags = event == JSON_TRUE ? FILE_ARCHIVED : 0;
    plan->seen |= SEEN_ARCHIVED;
    return true;
  }
  if (event == JSON_NUMBER && json_key(json, 3, "type")) {
    if (!parse_uint(json->value, TYPE_MAX, &number)) return fail(plan, PLAN_E_INVALID);
    f->type = (uint8_t)number;
    plan->seen |= SEEN_TYPE;
  }
  if (event == JSON_NUMBER && json_key(json, 3, "raw_size")) {
    if (!parse_uint(json->value, 0xFFFFFF, &f->raw_size)) return fail(plan, PLAN_E_INVALID);
    plan->seen |= SEEN_SIZE;
  }
  if (event != JSON_STRING) return true;
  if (json_key(json, 3, "slug")) {
    plan->seen |= SEEN_SLUG;
    return copy_value(json, plan->current_slug, sizeof(plan->current_slug), true) || fail(plan, PLAN_E_INVALID);
  }
  if (json_key(json, 3, "version")) {
    plan->seen |= SEEN_VERSION;
    return copy_value(json, plan->current_version, sizeof(plan->current_version), true) || fail(plan, PLAN_E_VERSION);
  }
  if (json_key(json, 3, "name_hex")) {
    plan->seen |= SEEN_NAME;
    memset(f->name, 0, VAR_NAME_SIZE);
    return hex_decode(json->value, f->name, VAR_NAME_SIZE, &plan->current_name_length) || fail(plan, PLAN_E_INVALID);
  }
  if (json_key(json, 3, "raw_sha256")) {
    uint8_t n;
    plan->seen |= SEEN_SHA;
    return (hex_decode(json->value, f->sha256, SHA_SIZE, &n) && n == SHA_SIZE) || fail(plan, PLAN_E_INVALID);
  }
  if (json_key(json, 3, "raw_url")) {
    plan->seen |= SEEN_URL;
    return copy_value(json, plan->current_url, sizeof(plan->current_url), true) || fail(plan, PLAN_E_INVALID);
  }
  return true;
}

static uint8_t plan_project(plan_t *plan)
{
  for (uint8_t i = 0; i < plan->project_count; i++)
    if (!strcmp(plan->projects[i].slug, plan->current_slug))
      return strcmp(plan->projects[i].version, plan->current_version) ? DB_NONE : i;
  if (plan->project_count == PLAN_MAX_PROJECTS) {
    fail(plan, PLAN_E_TOO_MANY_PROJECTS);
    return DB_NONE;
  }
  plan_project_t *p = &plan->projects[plan->project_count];
  memset(p, 0, sizeof(*p));
  strcpy(p->slug, plan->current_slug);
  strcpy(p->version, plan->current_version);
  return plan->project_count++;
}

static bool commit_file(plan_t *plan)
{
  plan_file_t *f = &plan->current;
  if (plan->seen != SEEN_ALL || !slug_valid(plan->current_slug) || !plan->current_version[0] ||
      !var_valid(f->type, f->name, plan->current_name_length))
    return fail(plan, PLAN_E_INVALID);
  var_display(plan->problem, f->type, f->name);
  if (f->raw_size < 2) return fail(plan, PLAN_E_INVALID);
  if (!var_archivable(f->type)) f->flags &= (uint8_t)~FILE_ARCHIVED;
  if (f->raw_size > VAR_RAW_MAX) return fail(plan, PLAN_E_TOO_LARGE);
  for (uint8_t i = 0; i < plan->file_count; i++)
    if (var_same(plan->files[i].type, plan->files[i].name, f->type, f->name)) return fail(plan, PLAN_E_DUPLICATE);
  if (plan->file_count == PLAN_MAX_FILES) return fail(plan, PLAN_E_TOO_MANY_FILES);
  if ((f->project = plan_project(plan)) == DB_NONE) return fail(plan, PLAN_E_INVALID);
  if (!take_filename(plan)) return fail(plan, PLAN_E_INVALID);
  plan->problem[0] = 0;
  plan->files[plan->file_count++] = *f;
  return true;
}

bool api_install(json_t *json, uint8_t event)
{
  plan_t *plan = json->arg;
  if (json->depth == 1 && event == JSON_STRING) {
    if (json_key(json, 1, "slug"))
      return copy_value(json, plan->root, sizeof(plan->root), true) || fail(plan, PLAN_E_INVALID);
    if (json_key(json, 1, "version"))
      return copy_value(json, plan->version, sizeof(plan->version), true) || fail(plan, PLAN_E_VERSION);
  }
  if (json->depth == 2 && event == JSON_STRING && json_key(json, 1, "capabilities")) plan->caps |= cap_bit(json->value);
  if (json->depth < 2 || !json_key(json, 1, "files")) return true;
  if (json->depth == 2 && event == JSON_OBJECT) {
    memset(&plan->current, 0, sizeof(plan->current));
    plan->current_slug[0] = plan->current_version[0] = plan->current_url[0] = 0;
    plan->seen = 0;
    return true;
  }
  if (json->depth == 2 && event == JSON_END) return commit_file(plan);
  if (json->depth == 3) return file_field(plan, json, event);
  return true;
}

bool api_install_done(plan_t *plan)
{
  if (plan->error) return false;
  if (!slug_valid(plan->root) || !plan->version[0] || !plan->file_count) return fail(plan, PLAN_E_INVALID);
  return true;
}

bool api_updates(json_t *json, uint8_t event)
{
  updates_t *u = json->arg;
  if (json->depth < 2 || !json_key(json, 1, "updates")) return true;
  if (json->depth == 2 && event == JSON_OBJECT) {
    memset(&u->current, 0, sizeof(u->current));
    u->slug[0] = 0;
    u->current.state = UPDATE_UNKNOWN;
    return true;
  }
  if (json->depth == 2 && event == JSON_END) {
    uint8_t i = db_find(u->db, u->slug);
    if (i != DB_NONE) u->items[i] = u->current;
    return true;
  }
  if (json->depth == 4 && event == JSON_STRING && json_key(json, 3, "capabilities"))
    u->current.caps |= cap_bit(json->value);
  if (json->depth != 3) return true;
  if (event == JSON_TRUE && json_key(json, 3, "taken_down")) u->current.state = UPDATE_TAKEN_DOWN;
  if (event != JSON_STRING) return true;
  if (json_key(json, 3, "slug")) return copy_value(json, u->slug, sizeof(u->slug), true);
  if (json_key(json, 3, "version")) {
    if (u->current.state == UPDATE_UNKNOWN && copy_value(json, u->current.latest, sizeof(u->current.latest), true))
      u->current.state = UPDATE_LATEST;
  }
  return true;
}

bool api_error(json_t *json, uint8_t event)
{
  if (json->depth == 1 && event == JSON_STRING && json_key(json, 1, "error"))
    copy_value(json, json->arg, MESSAGE_SIZE, false);
  return true;
}
