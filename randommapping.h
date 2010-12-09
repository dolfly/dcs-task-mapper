#ifndef _AREX_RANDOMMAPPING_H_
#define _AREX_RANDOMMAPPING_H_

#include "arextypes.h"

struct ae_random_parameters {
  int max_iterations;
  double constant;
  double task_exp;
  double pe_exp;
};

struct ae_mapping *ae_random_mapping(struct ae_mapping *map, double initial);
struct ae_random_parameters *ae_random_read_parameters(FILE *f);

#endif

