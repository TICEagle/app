#ifndef HOST_CHECK_H
#define HOST_CHECK_H

#include <stdio.h>

extern int failures;

#define CHECK(cond)                                                                                                    \
  do {                                                                                                                 \
    if (!(cond)) {                                                                                                     \
      failures++;                                                                                                      \
      fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                       \
    }                                                                                                                  \
  } while (0)

void test_var(void);
void test_json(void);
void test_api(void);
void test_db(void);
void test_plan(void);
void test_dl(void);
void test_http(void);
unsigned rng(void);

#endif
