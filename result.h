#ifndef _AE_RESULT_H_
#define _AE_RESULT_H_

#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#include "arextypes.h"

struct ae_result {
	double initial;
	double initial_time;
	int initial_memory;
	double best;
	double best_time;
	int best_memory;
	struct timeval start_time;
	struct timeval end_time;
	long long evals;
	ssize_t allocated;
	double *time;
	double *objective;
};

void ae_init_result(struct ae_mapping *map);
void ae_free_result(struct ae_mapping *map);
void ae_print_result(struct ae_mapping *map, struct ae_mapping *oldmap);

extern char *sa_output_file;

#endif
