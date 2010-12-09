#ifndef _AE_MAPPINGHEURISTICS_H_
#define _AE_MAPPINGHEURISTICS_H_

#include "mapping.h"
#include "optimization.h"

struct mh_heuristics {
	char *name;
	void (*f)(struct ae_mapping *newm, struct ae_mapping *oldm, double T,
		  struct optstate *os);
};

extern struct mh_heuristics mh_heuristics[];

#endif
