#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "mapping.h"
#include "arexbasic.h"
#include "task.h"
#include "pe.h"

double ae_communication_time(struct ae_arch *arch, int icid, int amount)
{
  struct ae_interconnect *ic;
  int cycles;

  assert(icid >= 0 && icid < arch->nics);

  ic = arch->ics[icid];

  cycles = ic->latency + (amount * 8 + ic->width - 1) / ic->width;

  return ((double) cycles) / ((double) ic->freq);
}


/*
 * Get time to compute N operations for a given PE
 */
double ae_pe_computation_time(struct ae_pe *pe, int noperations)
{
  return noperations / (pe->performance_factor * pe->freq);
}


double ae_task_computation_time(struct ae_mapping *map, int taskid)
{
  struct ae_pe *pe;
  int peid;

  assert(taskid >= 0 && taskid < map->ntasks);
  peid = map->mappings[taskid];

  assert(peid >= 0 && peid < map->arch->npes);
  pe = map->arch->pes[peid];

  return ae_pe_computation_time(pe, map->tasks[taskid]->weight);
}


void ae_copy_mapping(struct ae_mapping *newmap, struct ae_mapping *map)
{
	memcpy(newmap->mappings, map->mappings, sizeof(*map->mappings) * map->ntasks);
	memcpy(newmap->icpriorities, map->icpriorities, sizeof(*map->icpriorities) * map->arch->npes);
	if (map->taskpriorities) {
		if (newmap->taskpriorities == NULL)
			MALLOC_ARRAY(newmap->taskpriorities, map->ntasks);
		memcpy(newmap->taskpriorities, map->taskpriorities, sizeof(map->taskpriorities[0]) * map->ntasks);
	} else {
		if (newmap->taskpriorities)
			free(newmap->taskpriorities);
		newmap->taskpriorities = NULL;
	}
}


struct ae_mapping *ae_create_mapping(void)
{
  struct ae_mapping *map;
  if ((map = calloc(1, sizeof(*map))) == NULL)
    ae_err("ae_create_mapping: no memory for struct ae_mapping\n");
  return map;
}


struct ae_mapping *ae_fork_mapping(struct ae_mapping *map)
{
	struct ae_mapping *newmap;
	int ntasks = map->ntasks;

	newmap = ae_create_mapping();
	*newmap = *map;

	assert(map->tasks != NULL || map->processes != NULL);

	newmap->tasks = NULL;
	newmap->processes = NULL;
	if (map->tasks != NULL)
		newmap->tasks = ae_fork_memory(map->tasks, ntasks * sizeof(map->tasks[0]));
	if (map->processes != NULL)
		newmap->processes = ae_fork_memory(map->processes, ntasks * sizeof(map->processes[0]));
	if (map->taskpriorities != NULL)
		newmap->taskpriorities = ae_fork_memory(map->taskpriorities, sizeof(map->taskpriorities[0]) * ntasks);

	newmap->ntasksallocated = ntasks;

	newmap->mappings = ae_fork_memory(map->mappings, ntasks * sizeof(map->mappings[0]));
	newmap->icpriorities = ae_fork_memory(map->icpriorities, map->arch->npes * sizeof(map->icpriorities[0]));

	return newmap;
}


void ae_free_mapping(struct ae_mapping *map)
{
	free(map->tasks);
	free(map->processes);
	free(map->mappings);
	free(map->icpriorities);
	free(map->taskpriorities);
	memset(map, 0, sizeof(*map));
	free(map);
}

void ae_initialize_task_priorities(struct ae_mapping *map)
{
	int tid;
	for (tid = 0; tid < map->ntasks; tid++)
		ae_set_task_priority(map, tid, 0.0);
}

void ae_latency_costs(struct ae_mapping *map)
{
  int *target_pes;
  struct ae_schedule *s;
  struct ae_pe *pe;
  struct ae_task *task;
  double *l;
  int childid, parentid;
  int temp;
  int childpeid, parentpeid;
  int remotetargets, localtargets, sendbytes, cycles;

  s = map->schedule;
  l = s->latencies;

  MALLOC_ARRAY(target_pes, map->arch->npes);
  memset(target_pes, -1, sizeof(target_pes[0]) * map->arch->npes);

  AE_FOR_EACH_TASK(map, task, parentid) {

    parentpeid = map->mappings[parentid];
    remotetargets = 0;
    localtargets = 0;

    AE_FOR_EACH_CHILD(task, childid, temp) {
      childpeid = map->mappings[childid];
      if (childpeid != parentpeid) {
	if (target_pes[childpeid] != parentid) {
	  target_pes[childpeid] = parentid;
	  remotetargets++;
	}
      } else {
	localtargets++;
      }
    }
    assert(remotetargets <= task->nout);
    assert((task->nout == 0 && remotetargets == 0) || (task->nout > 0));
    pe = map->arch->pes[parentpeid];
    sendbytes = 0;
    if (task->nout > 0)
      sendbytes = task->outbytes[0];

    cycles = remotetargets * ae_pe_send_cost(sendbytes, pe);
    cycles += localtargets * ae_pe_copy_cost(sendbytes, pe);

    l[parentid] = ((double) cycles) / pe->freq;
    /* fprintf(stderr, "task %d latency %.9lf\n", parentid, l[parentid]); */
  }
  free(target_pes);
}


void ae_print_mapping(struct ae_mapping *map)
{
  int taskid;
  fprintf(stderr, "task mappings: ");
  for (taskid = 0; taskid < map->ntasks; taskid++)
    fprintf(stderr, "%d ", map->mappings[taskid]);
  ae_print_mapping_balance(stderr, map);
}


void ae_print_mapping_balance(FILE *f, struct ae_mapping *map)
{
  int peid, taskid;
  int *target_pes;
  CALLOC_ARRAY(target_pes, map->arch->npes);
  for (taskid = 0; taskid < map->ntasks; taskid++)
    target_pes[map->mappings[taskid]]++;
  fprintf(f, "balance: ");
  for (peid = 0; peid < map->arch->npes; peid++)
    fprintf(f, "%.3f ", ((float) target_pes[peid]) / map->ntasks);
  fprintf(f, "\n");
  free(target_pes);
}

void ae_randomize_mapping(struct ae_mapping *map)
{
  int taskid;

  for (taskid = 0; taskid < map->ntasks; taskid++)
    ae_set_mapping(map, taskid, ae_randi(0, map->arch->npes));
}

void ae_randomize_n_task_mappings(struct ae_mapping *m, int n)
{
	int peid;
	int taskid;
	int i;

	assert(n > 0);

	if (m->arch->npes == 1)
		return;

	for (i = 0; i < n; i++) {
		taskid = ae_randi(0, m->ntasks);
		peid = ae_randi(0, m->arch->npes - 1);
		if (peid >= m->mappings[taskid])
			peid++;
		ae_set_mapping(m, taskid, peid);
	}
}

int ae_set_mapping(struct ae_mapping *map, int tid, int peid)
{
  assert(tid >= 0 && tid < map->ntasks);

  if (map->isstatic[tid] == 0)
    map->mappings[tid] = peid;

  return map->mappings[tid];
}

void ae_randomize_task_priorities(struct ae_mapping *map)
{
	int i;
	int *priorities;
	int pos;

	MALLOC_ARRAY(priorities, map->ntasks);
	for (i = 0; i < map->ntasks; i++)
		priorities[i] = i;

	for (i = map->ntasks; i > 0; i--) {
		pos = ae_randi(0, i);
		ae_set_task_priority(map, i - 1, (double) priorities[pos]);
		priorities[pos] = priorities[i - 1];
	}

	free(priorities);
}

void ae_set_task_priority(struct ae_mapping *map, int tid, double pri)
{
	assert(tid >= 0 && tid < map->ntasks);
	if (map->taskpriorities == NULL)
		CALLOC_ARRAY(map->taskpriorities, map->ntasks);
	map->taskpriorities[tid] = pri;
}

double ae_specific_communication_time(struct ae_mapping *map, int icid, int srctask, int dsttask)
{
  if (map->mappings[srctask] == map->mappings[dsttask])
    return 0.0;

  return ae_communication_time(map->arch, icid, ae_task_send_amount(map, srctask, dsttask));
}

double ae_total_mappings(struct ae_mapping *map)
{
  int nstatics = 0;
  int taskid;
  for (taskid = 0; taskid < map->ntasks; taskid++) {
    if (map->isstatic[taskid])
      nstatics++;
  }

  return pow(map->arch->npes, map->ntasks - nstatics);
}

double ae_total_schedules(struct ae_mapping *map)
{
	double s = 1;
	int i;
	for (i = 2; i <= map->ntasks; i++)
		s *= i;
	return s;
}

void ae_zero_mapping(struct ae_mapping *map)
{
  int taskid;
  for (taskid = 0; taskid < map->ntasks; taskid++)
    ae_set_mapping(map, taskid, 0);
}
