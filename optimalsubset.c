/*
 *   DCS task mapper
 *   Copyright (C) 2004-2010 Tampere University of Technology (Heikki Orsila)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "optimalsubset.h"
#include "mapping.h"
#include "arexbasic.h"
#include "input.h"


struct ae_mapping *ae_osm(struct ae_mapping *map, struct ae_osm_parameters *p)
{
	struct ae_mapping *bestmap, *newmap;
	double bestcost, cost, initialcost;
	int taskid;
	int ntasks = map->ntasks;
	int npes = map->arch->npes;
	int peid;
	int *dynamic, *elected;
	int electable, i;
	int subsetsize, maxsubsetsize;
	long long iteration, round;

	assert(p->subsetsize > 0 && p->subsetsize <= ntasks);

	/* pick a random subset of dynamic tasks (non-static task mappings) */
	maxsubsetsize = p->subsetsize;

	MALLOC_ARRAY(dynamic, ntasks);
	MALLOC_ARRAY(elected, maxsubsetsize);

	electable = 0;

	for (taskid = 0; taskid < ntasks; taskid++) {
		if (map->isstatic[taskid] == 0)
			dynamic[electable++] = taskid;
	}

	assert(electable > 0 && electable <= ntasks);

	if (electable < maxsubsetsize)
		maxsubsetsize = electable;

	bestmap = ae_fork_mapping(map);
	bestcost = p->objective(bestmap);

	initialcost = bestcost;

	newmap = ae_fork_mapping(map);

	iteration = round = 0;

	subsetsize = 2;

	while (1) {

		double oldbestcost = bestcost;

		printf("best_osm_cost_so_far %lld %d %lld %.9lf %.3f\n", round, subsetsize, iteration, bestcost, initialcost / bestcost);

		/* pick 'subsetsize' tasks by random from 'electable' tasks */
		ae_random_cards(elected, subsetsize, electable);

		/* map elected table to tasks that are dynamic (non-static task mappings) */
		for (i = 0; i < subsetsize; i++) {
			elected[i] = dynamic[elected[i]];
			taskid = elected[i];
			newmap->mappings[taskid] = 0;
		}

		/* Brute force optimization on the random subset */
		while (1) {
			cost = p->objective(newmap);

			iteration++;

			if (cost < bestcost) {
				bestcost = cost;
				ae_copy_mapping(bestmap, newmap);
				printf("best_osm_cost_so_far %lld %d %lld %.9lf %.3f\n", round, subsetsize, iteration, bestcost, initialcost / bestcost);
			}

			for (i = 0; i < subsetsize; i++) {
				taskid = elected[i];
				peid = (newmap->mappings[taskid] + 1) % npes;
				newmap->mappings[taskid] = peid;
				if (peid)
					break;
			}

			if (i == subsetsize)
				break;
		}

		ae_copy_mapping(newmap, bestmap);

		round++;

		if (oldbestcost == bestcost) {
			if (subsetsize == maxsubsetsize)
				break;

			subsetsize++;

			if (subsetsize > maxsubsetsize)
				subsetsize = maxsubsetsize;
		} else {
			if (subsetsize >= 3) {
				/* oldbestcost > bestcost */
				subsetsize--;
			}
		}
	}

	free(dynamic);
	free(elected);
	return newmap;
}


void ae_osm_init(struct ae_osm_parameters *p, int ntasks, int npes)
{
	assert(ntasks > 0 && npes > 0);

	if (p->subsetsize == 0) {

		/* Choose subset size so that M^x = c * N^{c_N} * M^{c_M}, where M
		   is the number of PEs, N the number of tasks, and c, c_N and c_M are
		   arbitraty multipliers > 0 */
		p->subsetsize = log(p->c) / log((double) npes) + p->cN * log((double) ntasks) / log((double) npes) + p->cP;
		if (p->subsetsize < 2)
			p->subsetsize = 2;
	}

	if (p->subsetsize > ntasks)
		p->subsetsize = ntasks;

	fprintf(stderr, "osm subset size: %d\n", p->subsetsize);

	p->subsettries = (long long) pow((double) npes, (double) p->subsetsize);

	fprintf(stderr, "osm tries per round: %lld\n", p->subsettries);
}


struct ae_osm_parameters *ae_osm_read_parameters(FILE * f)
{
	struct ae_osm_parameters *p;
	if ((p = calloc(1, sizeof(p[0]))) == NULL)
		ae_err("%s: no memory\n", __func__);

	ae_match_word("multiplier", f);
	p->c = ae_get_double(f);
	assert(p->c > 0.0);
	printf("osm_multiplier: %lf\n", p->c);

	ae_match_word("task_exponent", f);
	p->cN = ae_get_double(f);
	assert(p->cN > 0.0);
	printf("osm_task_exponent: %lf\n", p->cN);

	ae_match_word("pe_exponent", f);
	p->cP = ae_get_double(f);
	assert(p->cP > 0.0);
	printf("osm_pe_exponent: %lf\n", p->cP);

	p->objective = NULL;

	p->subsetsize = 0;

	ae_match_word("subset_size", f);
	p->subsetsize = ae_get_int(f);
	assert(p->subsetsize >= 0);
	return p;
}
