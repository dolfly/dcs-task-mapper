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

#include "bruteforce.h"
#include "mapping.h"
#include "schedule.h"
#include "optimization.h"
#include "permutation.h"
#include "arexbasic.h"
#include "input.h"

#include <assert.h>

static void copy_permutation(struct ae_mapping *map, const struct permutation *p)
{
	int taskid;
	int i;
	/* Earlier tasks in permutation have higher priority than later tasks */
	for (i = 0; i < map->ntasks; i++) {
		taskid = p->permutation[i];
		map->taskpriorities[taskid] = map->ntasks - i;
	}
}

static int mapping_step(struct ae_mapping *map, struct permutation *p)
{
	int i;
	int newpe;

	for (i = 0; i < map->ntasks; i++) {
		if (map->isstatic[i])
			continue;
		newpe = (map->mappings[i] + 1) % map->arch->npes;
		if (ae_set_mapping(map, i, newpe) != 0)
			break;
	}

	return i == map->ntasks;
}

static int scheduling_step(struct ae_mapping *map, struct permutation *p)
{
	int ret = permutation_next(p);
	if (ret)
		permutation_reset(p);
	copy_permutation(map, p);
	return ret != 0;
}

int increment(struct ae_mapping *map, struct permutation *p, int flags)
{
	int ret;

	if ((flags & OPT_SCHEDULING) && (flags & OPT_SCHEDULING_FIRST)) {
		ret = scheduling_step(map, p);
		if (!ret)
			return 0;
	}

	if (flags & OPT_MAPPING) {
		ret = mapping_step(map, p);
		if (!ret)
			return 0;
	}

	if ((flags & OPT_SCHEDULING) && !(flags & OPT_SCHEDULING_FIRST))
		return scheduling_step(map, p);

	return 1;
}

struct ae_mapping *ae_brute_force(struct ae_mapping *oldmap, double initial, int flags)
{
	struct ae_mapping *map = ae_fork_mapping(oldmap);
	struct ae_mapping *bestmap;
	double bestcost;
	long long noptimums = 1;
	double cost;
	int p;
	int oldp = 0;
	long long i = 0;
	long long opti = 0;
	double maxi = 1;
	struct permutation *permutation = NULL;

	assert(!ae_config.find_maximum);

	if (flags & OPT_MAPPING) {
		ae_zero_mapping(map);
		maxi *= ae_total_mappings(map);
		printf("brute_force_mappings: %e\n", ae_total_mappings(map));
	}

	if (flags & OPT_SCHEDULING) {
		ae_initialize_task_priorities(map);
		MALLOC_ARRAY(permutation, 1);
		permutation_init(permutation, map->ntasks);
		copy_permutation(map, permutation);
		maxi *= ae_total_schedules(map);
		printf("brute_force_schedules: %e\n", ae_total_schedules(map));
	}

	printf("brute_force_iterations: %e\n", maxi);

	bestmap = ae_fork_mapping(map);
	bestcost = map->optimization->objective(bestmap);

	do {
		cost = map->optimization->objective(map);

		if (cost == bestcost)
			noptimums++;

		if (cost < bestcost) {
			bestcost = cost;
			opti = i;
			ae_copy_mapping(bestmap, map);
			noptimums = 1;
		}

		p = (i / maxi) * 100;
		if (p != oldp) {
			oldp = p;
			printf("p: %d%% i: %lld best_cost: %.9lf best_gain: %.3lf\n", p, i, bestcost, initial / bestcost);
		}
		i++;
	} while (!increment(map, permutation, flags));

	printf("noptimums: %lld\n", noptimums);
	printf("optimumiteration: %lld (%d%%)\n", opti, (int) (100 * (opti / maxi)));

	ae_free_mapping(map);

	if (permutation)
		permutation_free(permutation);
	free(permutation);

	return bestmap;
}
