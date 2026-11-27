#include <stdlib.h>
#include <string.h>

#include "check.h"
#include "json.h"

static char trace[4096];
static size_t events;
static size_t max_depth_seen;

static bool record(json_t *json, uint8_t event)
{
  static const char codes[] = "snTFNOAE";
  char item[JSON_VALUE_SIZE + JSON_KEY_SIZE + 16];
  events++;
  if (json->depth > max_depth_seen) max_depth_seen = json->depth;
  if (strlen(trace) > sizeof(trace) - sizeof(item)) return true;
  if (event <= JSON_NULL)
    snprintf(item, sizeof(item), "%u%c%s=%s;", json->depth, codes[event], json->keys[json->depth], json->value);
  else snprintf(item, sizeof(item), "%u%c%s;", json->depth, codes[event], json->keys[json->depth]);
  strcat(trace, item);
  return true;
}

static bool parse(const char *text, size_t chunk)
{
  json_t json;
  size_t n = strlen(text);
  trace[0] = 0;
  json_init(&json, record, NULL);
  for (size_t i = 0; i < n; i += chunk)
    if (!json_feed(&json, (const uint8_t *)text + i, i + chunk > n ? n - i : chunk)) return false;
  return json_finish(&json);
}

static void every_split(const char *text, const char *want)
{
  for (size_t chunk = 1; chunk <= strlen(text); chunk++) {
    CHECK(parse(text, chunk));
    CHECK(!strcmp(trace, want));
  }
}

static bool stop_at_depth(json_t *json, uint8_t event)
{
  (void)event;
  return json->depth < 2;
}

static void hostile(void)
{
  static const char *const bad[] = {"",
                                    "{",
                                    "}",
                                    "[1,]",
                                    "{\"a\" 1}",
                                    "{\"a\":1,}",
                                    "{\"a\":01x}",
                                    "nul",
                                    "truex",
                                    "\"abc",
                                    "\"a\x01\"",
                                    "\"\\x\"",
                                    "\"\\u12G4\"",
                                    "{} {}",
                                    "[1 2]",
                                    "{1:2}",
                                    "[\"a\":1]",
                                    "{\"a\":1]",
                                    "[[[[[[[[[1]]]]]]]]]",
                                    "-",
                                    "--1",
                                    "{\"a\":}",
                                    "[,1]",
                                    "\"\\u00",
                                    "{\"k\":\"v\"",
                                    "[1}",
                                    "{\"a\":1 \"b\":2}"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    bool ok = parse(bad[i], 1);
    if (ok) fprintf(stderr, "accepted: %s\n", bad[i]);
    CHECK(!ok);
  }
  char long_value[1000];
  memset(long_value, 'x', sizeof(long_value));
  long_value[0] = '"';
  long_value[sizeof(long_value) - 2] = '"';
  long_value[sizeof(long_value) - 1] = 0;
  CHECK(parse(long_value, 7));
  char long_number[400];
  memset(long_number, '1', sizeof(long_number) - 1);
  long_number[sizeof(long_number) - 1] = 0;
  CHECK(!parse(long_number, 3));
  json_t json;
  json_init(&json, stop_at_depth, NULL);
  CHECK(!json_feed(&json, (const uint8_t *)"[[[1]]]", 7));
  CHECK(!json_feed(&json, (const uint8_t *)"1", 1));
  CHECK(!json_finish(&json));
}

static void random_bytes(void)
{
  static const char base[] = "{\"projects\":[{\"slug\":\"gdash\",\"name\":\"G\\u00e9 \\\"D\\\"\",\"x\":[1,-2.5e3,true,"
                             "false,null,{}]}],\"more\":false}";
  char buf[sizeof(base)];
  json_t json;
  for (int round = 0; round < 200000; round++) {
    memcpy(buf, base, sizeof(base));
    for (int k = rng() % 4; k >= 0; k--) buf[rng() % (sizeof(base) - 1)] = (char)(rng() & 0xFF);
    json_init(&json, record, NULL);
    trace[0] = 0;
    size_t at = 0, n = sizeof(base) - 1;
    while (at < n) {
      size_t c = 1 + rng() % 16;
      if (c > n - at) c = n - at;
      if (!json_feed(&json, (const uint8_t *)buf + at, c)) break;
      at += c;
    }
    json_finish(&json);
  }
}

static void big_document(void)
{
  enum { FILES = 4096, FILE_SIZE = 900 };
  size_t cap = FILES * FILE_SIZE + 256, n = 0;
  char *doc = malloc(cap);
  json_t json;
  n += (size_t)sprintf(doc, "{\"slug\":\"root\",\"version\":\"1.0.0\",\"capabilities\":[],\"files\":[");
  for (int i = 0; i < FILES; i++)
    n += (size_t)sprintf(
        doc + n,
        "%s{\"slug\":\"project-%02d-xxxxxxxxxxxxxxxxxxxxxxxx\",\"version\":\"1.0.0-beta.1234567890\","
        "\"name\":\"A%04d\",\"name_hex\":\"4130303030303030\",\"type\":21,\"archived\":true,\"size\":65590,"
        "\"sha256\":\"%064d\",\"url\":\"/api/v1/dl/p/1.0.0/%064d\",\"raw_url\":\"/api/v1/dl/p/1.0.0/%064d"
        "?format=raw\",\"raw_size\":65507,\"raw_sha256\":\"%064d\",\"main\":true}",
        i ? "," : "", i % 64, i, i, i, i, i);
  n += (size_t)sprintf(doc + n, "]}");
  events = 0;
  max_depth_seen = 0;
  json_init(&json, record, NULL);
  trace[0] = 0;
  for (size_t at = 0; at < n; at += 536)
    CHECK(json_feed(&json, (const uint8_t *)doc + at, at + 536 > n ? n - at : 536));
  CHECK(json_finish(&json));
  CHECK(events == 8 + FILES * 15);
  CHECK(max_depth_seen == 3);
  printf("json: %zu byte install document with %d files parsed\n", n, FILES);
  free(doc);

  size_t desc = 12 * 1024;
  doc = malloc(desc * 20 + 256);
  n = (size_t)sprintf(doc, "{\"slug\":\"big\",\"description\":\"");
  for (size_t i = 0; i < desc; i++) n += (size_t)sprintf(doc + n, "\\u003c");
  n += (size_t)sprintf(doc + n, "\",\"description_html\":\"");
  for (size_t i = 0; i < desc; i++) n += (size_t)sprintf(doc + n, "&lt;\\u0026");
  n += (size_t)sprintf(doc + n, "\",\"latest\":\"1.0.0\"}");
  json_init(&json, record, NULL);
  trace[0] = 0;
  CHECK(json_feed(&json, (const uint8_t *)doc, n));
  CHECK(json_finish(&json));
  CHECK(strstr(trace, "1slatest=1.0.0;") != NULL);
  printf("json: %zu byte project document parsed\n", n);
  free(doc);
}

void test_json(void)
{
  every_split("{\"a\":1,\"b\":[true,false,null,\"x\"],\"c\":{\"d\":-2.5e+3}}",
              "0O;1na=1;1Ab;2T=true;2F=false;2N=null;2s=x;1Eb;1Oc;2nd=-2.5e+3;1Ec;0E;");
  every_split(" [ ] ", "0A;0E;");
  every_split("\"\\\"\\\\\\/\\n\\u0041\\u00e9\\u4e2d\"", "0s=\"\\/ A??;");
  every_split("\"caf\xc3\xa9 \xe4\xb8\xad\"", "0s=caf? ?;");
  every_split("{\"a_very_long_key_name_over_limit\":1,\"k\":2}", "0O;1n=1;1nk=2;0E;");
  every_split("42", "0n=42;");
  hostile();
  random_bytes();
  big_document();
}
