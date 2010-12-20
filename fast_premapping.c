/*
 * DCS task mapper
 * Copyright (C) 2004-2010 Tampere University of Technology
 *
 * The program was originally written by Heikki Orsila <heikki.orsila@iki.fi>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <assert.h>

#include "fast_premapping.h"
#include "arexbasic.h"
#include "mapping.h"
#include "optimization.h"
#include "result.h"


struct ae_mapping *ae_fast_premapping(struct ae_mapping *original_map)
{
    struct ae_mapping *map;
    char *visited;
    int i;
    int taskid;
    struct ae_task *task, *nexttask;
    int *lifo;
    int lifosize;
    int *adjacenttasks;
    double objective;

    map = ae_fork_mapping(original_map);

    CALLOC_ARRAY(visited, map->ntasks);
    MALLOC_ARRAY(lifo, map->ntasks);
    MALLOC_ARRAY(adjacenttasks, map->ntasks);

    for (i = 0; i < map->ntasks; i++)
	ae_set_mapping(map, i, 0);

    AE_FOR_EACH_TASK(map, task, taskid) {
	if (task->nout == 0) {

	    /* Do not process visited tasks */
	    if (visited[taskid])
		continue;

	    lifosize = 1;
	    lifo[0] = taskid;

	    while (lifosize > 0) {
		nexttask = map->tasks[lifo[lifosize - 1]];
		lifosize--;

		if (nexttask->nin == 0)
		    continue;

		/* Insert adjacent tasks (parents) into lifo in random order */
		ae_random_cards(adjacenttasks, nexttask->nin, nexttask->nin);

		for (i = 0; i < nexttask->nin; i++) {
		    int adjid = adjacenttasks[i];
		    int adjpeid;

		    /* Do not insert visited tasks into lifo */
		    if (visited[adjid])
			continue;

		    /* First parent (as decided by ae_random_cards() inherits
		       pe from the child, others get random pe */
		    if (i == 0) {
			adjpeid = map->mappings[taskid];
		    } else {
			adjpeid = ae_randi(0, map->arch->npes);
		    }

		    ae_set_mapping(map, adjid, adjpeid);

		    assert(lifosize < map->ntasks);
		    lifo[lifosize++] = adjid;
		}
	    }
	}
    }

    free(adjacenttasks);
    free(lifo);
    free(visited);

    objective = map->optimization->objective(map);

    fprintf(stderr, "objective after fast premapping: %.9f (gain %.3f)\n",
	    objective, map->result->initial / objective);

    return map;
}
