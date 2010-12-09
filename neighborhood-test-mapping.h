#ifndef _AREX_NEIGHBORHOOD_TEST_MAPPING_H_
#define _AREX_NEIGHBORHOOD_TEST_MAPPING_H_

#include "arextypes.h"
#include <stdio.h>

struct ae_ntm_parameters {
	int changemax;
	size_t itermax;
};

struct ae_mapping *ae_neighborhood_test_mapping(struct ae_mapping *map, double initial);
struct ae_ntm_parameters *ae_ntm_read_parameters(FILE *f);

#endif

