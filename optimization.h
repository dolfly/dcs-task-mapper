#ifndef _AE_OPTIMIZATION_H_
#define _AE_OPTIMIZATION_H_

#include "arextypes.h"

#include <stdio.h>

#define OPT_MAPPING          1
#define OPT_SCHEDULING       2
#define OPT_SCHEDULING_FIRST 4

struct optmoveprobabilities {
	double pworse;
	double psame;
	double pbetter;
};

struct optmove {
	double acceptedobjective;
	double newobjective;
};

struct optstate {
	size_t ringsize;
	size_t ringpos; /* index of the latest objectve if ringused > 0 */
	size_t ringused;
	struct optmove *moves;
};

struct ae_optimization {
	char *method_name;
	struct ae_mapping * (*method)(struct ae_mapping *map, double initial);
	double (*objective)(struct ae_mapping *map);
	void *params;

	double power_k;
	char *objective_name;
};

struct ae_optimization *ae_create_optimization_context(void);
struct ae_mapping *ae_optimize(struct ae_mapping *map);
void ae_read_optimization_parameters(struct ae_mapping *map, FILE *f);

void append_optstate_move(struct optstate *os, double old, double new);
double cost_diff(double old, double new);
int opt_move_probabilities(struct optmoveprobabilities *ps, struct optstate *os);
struct optstate *create_optstate(size_t ringsize);
void free_optstate(struct optstate *os);

#endif
