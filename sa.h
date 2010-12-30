#ifndef _AE_SA_H_
#define _AE_SA_H_

#include "arextypes.h"
#include "optimization.h"

enum ae_sa_autotemp_mode {
	AE_SA_AUTOTEMP_OLD = 1,
	AE_SA_AUTOTEMP,
};

struct ae_sa_parameters;

struct ae_sa_parameters {
	int max_rejects;
	double ref_E;
	int schedule_max;
	double schedule_param1;
	double acceptor_param1;

	int ztpset; /* Ignore zero_transition_prob if ztpset == 0 */
	double zero_transition_prob;

	double T0;
	double Tf;
	double Tt;

	char *heuristics_name;

	int greedy;

	enum ae_sa_autotemp_mode autotemp_mode; /* 0 means disabled */
	double autotemp_k;

	int maxpes;
	int leveloptimization;

	double (*acceptor)(double dE, double T, struct ae_sa_parameters *params);
	void (*move)(struct ae_mapping *newmap, struct ae_mapping *map, double T, struct optstate *os);
	double (*objective)(struct ae_mapping *map);
	double (*schedule)(double T, struct ae_sa_parameters *params);
};

struct sa_level {
	double objective;
	double T;
};

void ae_sa_autotemp(struct ae_sa_parameters *params, struct ae_mapping *map);
struct ae_sa_parameters *ae_sa_read_parameters(FILE *f, struct ae_mapping *map);
struct ae_mapping *ae_sa(struct sa_level *salevels, size_t maxlevels, struct ae_mapping *S0, double T0, struct ae_sa_parameters *params);
void ae_sa_set_heuristics(struct ae_sa_parameters *p, char *name);

#endif
