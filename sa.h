#ifndef _AE_SA_H_
#define _AE_SA_H_

#include "arextypes.h"
#include "optimization.h"

struct ae_sa_parameters;

struct ae_sa_parameters {
	int max_rejects;
	double ref_E;
	int schedule_max;
	double schedule_param1;
	double acceptor_param1;
	double zero_transition_prob;
	double T0;
	double Tf;

	char *heuristics_name;

	int greedy;
	int autotemp;

	int maxpes;
	int leveloptimization;

	double (*acceptor)(double dE, double T, struct ae_sa_parameters *params);
	void (*move)(struct ae_mapping *newmap, struct ae_mapping *map, double T, struct optstate *os);
	double (*objective)(struct ae_mapping *map);
	double (*schedule)(double T, int k, struct ae_sa_parameters *params);
};

struct sa_level {
	double objective;
	double T;
};

void ae_sa_autotemp(struct ae_sa_parameters *params, struct ae_mapping *map);
double ae_sa_geometric_schedule(double T, int k, struct ae_sa_parameters *params);
struct ae_sa_parameters *ae_sa_read_parameters(FILE *f);
struct ae_mapping *ae_sa(struct sa_level *salevels, size_t maxlevels, struct ae_mapping *S0, double T0, struct ae_sa_parameters *params);
void ae_sa_set_heuristics(struct ae_sa_parameters *p, char *name);

#endif
