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
#include <string.h>

#include "arexbasic.h"
#include "kpn.h"
#include "input.h"
#include "pe.h"
#include "mapping.h"
#include "schedule.h"

static struct intarray *get_fifo(struct kpn_process_state *pstate, int taskid)
{
	size_t i;
	size_t nsources = intarray_len(&pstate->sources);
	for (i = 0; i < nsources; i++) {
		if (intarray_get(&pstate->sources, i) == taskid)
			break;
	}
	if (i == nsources)
		ae_err("KPN source %d not found\n", taskid);
	return pstate->fifos[i];
}

static int kpn_read(struct kpn_process_state *pstate, int src)
{
	struct intarray *fifo = get_fifo(pstate, src);
	if (intarray_len(fifo) == 0) {
		pstate->blocked = src;
		return 0;
	}
	return intarray_pop_head(fifo);
}

static int kpn_write(int dst, int src, unsigned int amount,
		     struct ae_mapping *map)
{
	struct ae_pe *pe = map->arch->pes[map->mappings[src]];

	if (map->mappings[src] == map->mappings[dst]) {
		return ae_pe_copy_cost(amount, pe);
	} else {
		return ae_pe_send_cost(amount, pe);
	}
}

int ae_kpn_execute(struct kpn_event *event,
		   struct kpn_state *kpnstate,
		   int taskid,
		   struct ae_mapping *map)
{
	struct kpn_process_state *pstate = &kpnstate->pstates[taskid];
	struct kpn_process *p = map->processes[taskid];
	struct kpn_inst *inst;
	struct ae_pe *pe = map->arch->pes[map->mappings[taskid]];
	int cycles = 0;
	int amount;

	if (pstate->programcounter >= p->ninsts || pstate->blocked >= 0)
		return 0;

	inst = &p->insts[pstate->programcounter];

	switch (inst->type) {
	case KPN_COMPUTE:
		cycles = inst->amount;
		break;

	case KPN_READ:
		assert(inst->src < map->ntasks);
		amount = kpn_read(pstate, inst->src);
		assert(amount >= 0);
		if (amount == 0)
			return 0;

		cycles = ae_pe_copy_cost(amount, pe);
		break;

	case KPN_WRITE:
		assert(inst->dst < map->ntasks);
		cycles = kpn_write(inst->dst, taskid, inst->amount, map);
		break;

	default:
		ae_err("Process %d unknown inst type %d\n", taskid, inst->type);
	}

	pstate->programcounter++;

	event->duration = ae_pe_computation_time(pe, MAX(cycles, 1));
	event->inst = inst;
	return 1;
}

int ae_kpn_unblock(struct kpn_state *kpnstate, const struct kpn_inst *inst)
{
	struct kpn_process_state *pstate = &kpnstate->pstates[inst->dst];
	struct intarray *fifo = get_fifo(pstate, inst->src);

	intarray_append(fifo, (int) inst->amount);

	if (pstate->blocked != inst->src)
		return 0;

	/* Unblock the process */
	pstate->blocked = -1;
	return 1;
}

void ae_free_kpn_state(struct kpn_state *kpnstate, struct ae_mapping *map)
{
	int taskid;
	size_t i;
	size_t nsources;
	struct kpn_process_state *pstate;

	for (taskid = 0; taskid < map->ntasks; taskid++) {
		pstate = &kpnstate->pstates[taskid];

		nsources = intarray_len(&pstate->sources);
		for (i = 0; i < nsources; i++) {
			intarray_free(pstate->fifos[i]);
			pstate->fifos[i] = NULL;
		}

		free(pstate->fifos);
		pstate->fifos = NULL;

		intarray_free(&pstate->sources);
	}

	free(kpnstate->pstates);
	kpnstate->pstates = NULL;
}

void ae_init_kpn_state(struct kpn_state *kpnstate, struct ae_mapping *map)
{
	int taskid;
	size_t i;
	size_t nsources;
	struct kpn_process_state *pstate;
	struct kpn_process *p;
	struct intarray *sources;
	int src;

	memset(kpnstate, 0, sizeof(*kpnstate));

	CALLOC_ARRAY(kpnstate->pstates, map->ntasks);

	/* Initialize source list for each process */
	for (taskid = 0; taskid < map->ntasks; taskid++) {
		pstate = &kpnstate->pstates[taskid];
		pstate->blocked = -1;
		intarray_initialize(&pstate->sources, 0);
	}

	/* Analyze sources for each process */
	for (taskid = 0; taskid < map->ntasks; taskid++) {
		p = map->processes[taskid];

		kpnstate->ninstsleft += p->ninsts;

		for (i = 0; i < p->ninsts; i++) {
			if (p->insts[i].type != KPN_READ)
				continue;

			sources = &kpnstate->pstates[taskid].sources;

			src = p->insts[i].src;

			if (intarray_search(src, sources) >= 0)
				continue;

			intarray_append(sources, src);
		}
	}

	/* Initialize fifos for each process based on sources */
	for (taskid = 0; taskid < map->ntasks; taskid++) {
		pstate = &kpnstate->pstates[taskid];
		sources = &pstate->sources;

		nsources = intarray_len(sources);
		MALLOC_ARRAY(pstate->fifos, nsources);

		for (i = 0; i < nsources; i++)
			pstate->fifos[i] = intarray_create(0);
	}
}

static int compare_double(const void *a, const void *b)
{
	double *x = (double *) a;
	double *y = (double *) b;
	if (x < y)
		return -1;
	if (y < x)
		return 1;
	return 0;
}

void ae_kpn_autotemp(struct ae_sa_parameters *params,
		     double minperf,
		     double maxperf,
		     double k,
		     struct ae_mapping *map)
{
	const int pivotpercentage = 5;

	int i;
	int j;
	double time;
	double maxtime = 0.0;
	double mintime = 1E10;
	double maxsum = 0.0;
	double minsum = 0.0;
	double *cycles;
	double pivotvalue;
	struct kpn_process *p;
	struct kpn_inst *inst;

	/* For each process, compute sum of computation cycles */
	MALLOC_ARRAY(cycles, map->ntasks);
	for (i = 0; i < map->ntasks; i++) {
		p = map->processes[i];
		cycles[i] = 0.0;
		for (j = 0; j < p->ninsts; j++) {
			inst = &p->insts[j];
			if (inst->type == KPN_COMPUTE)
				cycles[i] += inst->amount;
		}
	}

	/* Note, sorting cycles array does not harm this algorithm */
	qsort(cycles, map->ntasks, sizeof(cycles[0]), compare_double);

	for (i = 0; i < map->ntasks; i++) {
		time = cycles[i] / maxperf;
		mintime = MIN(mintime, time);
		minsum += time;

		time = cycles[i] / minperf;
		maxtime = MAX(maxtime, time);
		maxsum += time;
	}

	pivotvalue = cycles[(map->ntasks * pivotpercentage) / 100] / maxperf;
	mintime = MAX(mintime, pivotvalue);

	free(cycles);
	cycles = NULL;

	mintime = MAX(mintime, 1.0 / maxperf);
	assert(maxtime > 0.0);

	params->T0 = MIN(k * maxtime / minsum, 1.0);
	params->Tf = MIN(mintime / (k * maxsum), 1.0);

	printf("SA_autotemp: k: %e T0: %.9f Tf: %.9f\n", k, params->T0, params->Tf);
}

static void read_inst(struct kpn_inst *inst, int src, FILE *f)
{
	char *cmd = ae_get_word(f);
	if (strcmp(cmd, "c") == 0) {
		inst->type = KPN_COMPUTE;
		inst->amount = ae_get_uint(f);
		assert(inst->amount > 0);
	} else if (strcmp(cmd, "r") == 0) {
		inst->type = KPN_READ;
		inst->src = ae_get_int(f);
	} else if (strcmp(cmd, "w") == 0) {
		inst->type = KPN_WRITE;
		inst->src = src;
		inst->dst = ae_get_int(f);
		inst->amount = ae_get_uint(f);
		assert(inst->amount > 0);
	} else {
		ae_err("Unknown KPN command: %s\n", cmd);
	}
}

static void init_schedule(struct ae_mapping *map)
{
	map->appmodel->schedule = ae_schedule_kpn;
}

void ae_read_kpn(struct ae_mapping *map, FILE *f)
{
	char *word;
	unsigned int nodeid = 0;
	struct kpn_process *p;
	unsigned int i;
	unsigned int j;
	struct kpn_inst *inst;
	int ninsts = 0;
	int ncycles = 0;
	int nbytes = 0;

	map->appmodel->name = "kpn";

	map->appmodel->init_schedule = init_schedule;

	while (1) {
		word = ae_get_word(f);
		if (strcmp(word, "end_kpn") == 0)
			break;
		free(word);
		word = NULL;

		CALLOC_ARRAY(p, 1);
		p->id = ae_get_uint(f);
		assert(nodeid == p->id);
		nodeid++;
		p->ninsts = ae_get_uint(f);
		ninsts += p->ninsts;

		CALLOC_ARRAY(p->insts, p->ninsts);
		for (i = 0; i < p->ninsts; i++)
			read_inst(&p->insts[i], p->id, f);

		ae_add_resource(&map->ntasks, &map->ntasksallocated, (void ***) &map->processes, p);
	}
	free(word);

	for (i = 0; i < map->ntasks; i++) {
		p = map->processes[i];
		for (j = 0; j < p->ninsts; j++) {
			inst = &p->insts[j];

			/* Sanity checking for task numbers */
			if (inst->src < 0 ||
			    inst->dst < 0 ||
			    inst->src >= map->ntasks ||
			    inst->dst >= map->ntasks) {
				ae_err("KPN process %u has an invalid source or destination\n", i);
			}

			/*
			 * Fill unused fields with false data to find
			 * bugs more likely, and get statistics
			 */
			if (inst->type == KPN_COMPUTE) {
				inst->src = -1;
				inst->dst = -1;

				ncycles += inst->amount;

			} else if (inst->type == KPN_READ) {
				inst->dst = -1;
				inst->amount = -1;
			} else if (inst->type == KPN_WRITE) {
				nbytes += inst->amount;
			}
		}
	}

	printf("kpn_insts: %d\n", ninsts);
	printf("kpn_cycles: %d\n", ncycles);
	printf("kpn_bytes: %d\n", nbytes);
}
