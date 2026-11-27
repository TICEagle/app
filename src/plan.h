#ifndef CEAGLE_PLAN_H
#define CEAGLE_PLAN_H

#include "api.h"
#include "db.h"

#define PLAN_VAR_OVERHEAD 32u
#define PLAN_RAM_RESERVE 4096u

enum { PROBE_ABSENT, PROBE_SAME, PROBE_DIFFERENT, PROBE_LIBRARY, PROBE_RESERVED, PROBE_SELF };

typedef uint8_t (*var_probe_t)(uint8_t type, const uint8_t *name, const uint8_t *sha256);

uint8_t plan_check(plan_t *plan, db_t *db, var_probe_t probe);

#endif
