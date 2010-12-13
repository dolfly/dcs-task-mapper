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
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "task.h"
#include "arextypes.h"
#include "arexbasic.h"
#include "input.h"
#include "datastructures.h"
#include "mapping.h"
#include "result.h"

/* #define TOPOLOGICAL_DEBUG */

static void ae_analyze_task_inputs(struct ae_mapping *map)
{
  struct intlistarray ila;
  struct ae_task *task;
  int childid, parentid, taskid;
  int tempint;
  int i, j, k, n;
  int tresultoff;
  struct ae_taskresult *result;

  ae_init_ila(&ila, map->ntasks);
  AE_FOR_EACH_TASK(map, task, parentid) {
    AE_FOR_EACH_CHILD(task, childid, tempint) {
      ae_add_to_ila(&ila, childid, parentid);
    }
  }

  AE_FOR_EACH_TASK(map, task, taskid) {
    n = ila.nentries[taskid];
    if (n > 0) {
      MALLOC_ARRAY(task->in, n);
      task->nin = n;
      for (i = 0; i < n; i++)
	task->in[i] = ila.entries[taskid][i];
    }
  }

  ae_free_ila(&ila);

  MALLOC_ARRAY(map->tresultoffsets, map->ntasks);
  tresultoff = 0;
  for (i = 0; i < map->ntasks; i++) {
    map->tresultoffsets[i] = tresultoff;
    tresultoff += map->tasks[i]->nresult;
  }
  map->ntresults = tresultoff;
  MALLOC_ARRAY(map->tresultinv, map->ntresults);
  MALLOC_ARRAY(map->tresults, map->ntresults);
  k = 0;
  for (i = 0; i < map->ntasks; i++) {
    for (j = 0; j < map->tasks[i]->nresult; j++) {
      map->tresults[k] = &map->tasks[i]->result[j];
      map->tresultinv[k] = i;
      k++;
    }
  }
  assert(k == map->ntresults);

  AE_FOR_EACH_TASK(map, task, taskid) {
    task->ntresin = 0;
  }
  AE_FOR_EACH_TASK(map, task, taskid) {
    for (i = 0; i < task->nresult; i++) {
      result = &task->result[i];
      for (j = 0; j < result->ndst; j++) {
	map->tasks[result->dst[j]]->ntresin++;
      }
    }
  }
}


static int int_compare(void *a, void *b)
{
  int *ia = (int *) a;
  int *ib = (int *) b;
  if (*ia < *ib)
    return 1;
  if (*ib < *ia)
    return -1;
  return 0;
}


/* Input format is:
   <taskid> "out" <number of results>
   FOR_EACH_RESULT { <bytes> <number of targets> FOR_EACH_TARGET { <taskid> } }
   "weight" <operation complexity>
*/
static struct ae_task *ae_get_task_string(FILE *f, int taskid)
{
  struct ae_task *task;
  int i, j, k;
  struct ae_taskresult *res;
  struct ae_heap heap;
  int oldid, newid;

  CALLOC_ARRAY(task, 1);

  task->id = ae_get_int(f);
  assert(task->id >= 0);
  if (task->id != taskid) {
    ae_err("task numbers in input file must be numbered sequentially: 0, 1, "
	   "...\ntask %d in input file should have been numbered as %d\n",
	   task->id, taskid);
  }

  ae_match_word("out", f);
  task->nresult = ae_get_int(f);
  assert(task->nresult >= 0);

  CALLOC_ARRAY(task->result, task->nresult);

  ae_heap_init(&heap, int_compare, sizeof(int));

  for (i = 0; i < task->nresult; i++) {
    res = &task->result[i];

    res->bytes = ae_get_int(f);
    assert(res->bytes > 0);

    res->ndst = ae_get_int(f);
    assert(res->ndst > 0);

    MALLOC_ARRAY(res->dst, res->ndst);

    for (j = 0; j < res->ndst; j++) {
      res->dst[j] = ae_get_int(f);
      assert(res->dst[j] >= 0);
      ae_heap_insert(&heap, &res->dst[j]);
    }
  }

  /* get all unique taskid targets (from all results) by using heap */
  MALLOC_ARRAY(task->out, heap.n);
  oldid = -1;
  task->nout = 0;

  while (heap.n > 0) {
    ae_heap_extract_max(&newid, &heap);

    if (newid > oldid)
      task->out[task->nout++] = newid;

    oldid = newid;
  }

  assert(heap.n == 0);

  ae_heap_free(&heap);

  assert((task->nout == 0 && task->nresult == 0) || (task->nout > 0 && task->nresult > 0));

  /* Compute amount of bytes sent to each destination task */
  CALLOC_ARRAY(task->outbytes, task->nout);
  for (i = 0; i < task->nout; i++) {

    for (j = 0; j < task->nresult; j++) {
      res = &task->result[j];
      for (k = 0; k < res->ndst; k++) {
	if (res->dst[k] == task->out[i]) {
	  task->outbytes[i] += res->bytes;
	  break;
	}
      }
    }
  }

  ae_match_word("weight", f);
  task->weight = ae_get_double(f);
  assert(task->weight > 0.0);

  return task;
}


static void init_sendinfo_list(struct ae_sendinfo_list *l)
{
	l->n = 0;
	l->allocated = 0;
	l->sendinfos = NULL;
}


static void init_schedule(struct ae_mapping *map)
{
	struct ae_schedule *s = map->schedule;
	int i;

	map->appmodel->graph_stats = ae_stg_graph_stats;

	map->appmodel->schedule = ae_schedule_stg;

	MALLOC_ARRAY(s->tsort, map->ntasks);

	ae_topological_sort(s->tsort, map);

	MALLOC_ARRAY(s->pri, map->ntasks);
	MALLOC_ARRAY(s->latencies, map->ntasks);

	MALLOC_ARRAY(s->resultpartition, map->ntresults);
	s->resparts = NULL;
	s->respartsallocated = 0;

	MALLOC_ARRAY(s->tasksendinfos, map->ntasks);
	for (i = 0; i < map->ntasks; i++)
		init_sendinfo_list(&s->tasksendinfos[i]);

	CALLOC_ARRAY(s->result_refs, map->ntresults);
}


int ae_task_edges(struct ae_mapping *map)
{
  int nedges = 0;
  struct ae_task *task;
  int childid, parentid, temp;

  AE_FOR_EACH_TASK(map, task, parentid) {
    AE_FOR_EACH_CHILD(task, childid, temp) {
      nedges++;
    }
  }
  return nedges;
}


/* fix me. it must handle different results properly */

int ae_task_send_amount(struct ae_mapping *map, int srctask, int dsttask)
{
  int i;
  struct ae_task *task = map->tasks[srctask];
  for (i = 0; i < task->nout; i++) {
    if (task->out[i] == dsttask)
      return task->outbytes[i];
  }
  ae_err("no send found from %d to %d\n", task->id, dsttask);
  return 0;
}


/* topological sort aka children first sort. produces an array of tasks
   so that if task 'a' is data dependent on task 'b' ('b' is some ancestor of
   'a') then 'a' is in the array before 'b'. this is used to calculate
   b-level priorities efficiently.

   the algorithm is:
   1. initially tsort list is empty. nodes are added to the end of the list.
   2. find out all exit nodes. these can be put on the beginning of the
      tsort list, because they are data dependent on everyone else.
   3. start from the beginning of the tsort list, and check which nodes have
      parents whose all immediate children are in tsort list (jacob's
      father's sons). thus adding these parents to the list preserves the
      topological condition. the idea is to start with children, and add
      all parents to the list, whose children are already in the list.
   4. finished when all nodes are in tsort list.
*/
void ae_topological_sort(int *tsort, struct ae_mapping *map)
{
  int *outcount;
  int tsortind, tsortex;
  int temp;
  int parentid, taskid;
  struct ae_task *task;
  int ntasks = map->ntasks;

  if ((outcount = calloc(ntasks, sizeof(outcount[0]))) == NULL)
    ae_err("not enough memory for outcount list\n");

  tsortind = 0;

  /* copy exit nodes to tsort */
  AE_FOR_EACH_TASK(map, task, taskid) {
    if (task->nout != 0)
      continue;
    tsort[tsortind++] = taskid;
  }

  for (tsortex = 0; tsortex < tsortind; tsortex++) {
    task = map->tasks[tsort[tsortex]];
    /* move all parents whose all children are in tsort */
    AE_FOR_EACH_PARENT(task, parentid, temp) {
      outcount[parentid]++;
      if (map->tasks[parentid]->nout == outcount[parentid])
	tsort[tsortind++] = parentid;
    }
  }

  assert(tsortex == tsortind);
  assert(tsortex == ntasks);

#ifdef TOPOLOGICAL_DEBUG
  fprintf(stderr, "topological sort of %d tasks: ", ntasks);
  for (i = 0; i < ntasks; i++)
    fprintf(stderr, "%d ", tsort[i]);
  fprintf(stderr, "\n");
#endif
  free(outcount);
}

void ae_read_stg(struct ae_mapping *map, FILE *f)
{
  int ntasks;
  int i;
  struct ae_task *task;

  map->appmodel->name = "stg";

  map->appmodel->init_schedule = init_schedule;

  ntasks = ae_get_int(f);
  assert(ntasks > 0);

  for (i = 0; i < ntasks; i++) {
    ae_match_word("task", f);

    task = ae_get_task_string(f, i);

    ae_add_resource(&map->ntasks, &map->ntasksallocated, (void ***) &map->tasks, task);
    }

    ae_analyze_task_inputs(map);
}

void ae_stg_sa_autotemp(struct ae_sa_parameters *params,
			double minperf,
			double maxperf,
			double k,
			struct ae_mapping *map)
{
  int i;
  double time;
  double maxtime = 0.0;
  double mintime = 1E10;
  double maxsum = 0.0;
  double minsum = 0.0;
  double weight;

  for (i = 0; i < map->ntasks; i++) {
    weight = map->tasks[i]->weight;

    time = weight / maxperf;
    mintime = MIN(mintime, time);
    minsum += time;

    time = weight / minperf;
    maxtime = MAX(maxtime, time);
    maxsum += time;
  }

  params->T0 = MIN(k * maxtime / minsum, 1.0);
  params->Tf = MIN(mintime / (k * maxsum), 1.0);

  printf("SA_autotemp: k: %e T0: %.9f Tf: %.9f\n", k, params->T0, params->Tf);
}
