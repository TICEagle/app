#include "check.h"

int failures;
static unsigned state = 12345;

unsigned rng(void)
{
  state = state * 1103515245u + 12345u;
  return state >> 8;
}

int main(void)
{
  test_var();
  test_json();
  test_api();
  test_db();
  test_plan();
  test_dl();
  test_http();
  if (failures) {
    printf("FAIL: %d checks failed\n", failures);
    return 1;
  }
  printf("PASS\n");
  return 0;
}
