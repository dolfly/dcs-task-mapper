#ifndef _AE_OPTIMALSUBSET_H_
#define _AE_OPTIMALSUBSET_H_

#include "arextypes.h"

struct ae_osm_parameters {
	double c;
	double cN;
	double cP;
	int subsetsize;
	long long subsettries;
	double (*objective) (struct ae_mapping * map);
};

struct ae_mapping *ae_osm(struct ae_mapping *map, struct ae_osm_parameters *p);
void ae_osm_init(struct ae_osm_parameters *p, int ntasks, int npes);
struct ae_osm_parameters *ae_osm_read_parameters(FILE * f);

#endif
