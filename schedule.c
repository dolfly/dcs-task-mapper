#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "schedule.h"
#include "arexbasic.h"
#include "mapping.h"
#include "task.h"
#include "pe.h"
#include "interconnect.h"
#include "kpn.h"

#define STATE_CHECK_EVENTS (1)
#define STATE_FINISHED (2)

#define RESULT_ID(map, taskid, resind) ((map)->tresultoffsets[(taskid)] + (resind))
#define RESULT_SEND_INFO(map, resultid) ((struct ae_sendinfo *) &(map)->schedule->resparts[map->schedule->resultpartition[(resultid)]])


enum ae_eventtype {
	AE_EVENT_COMM_FIN = 1,
	AE_EVENT_COMP_FIN,
	AE_EVENT_IC_READY,
	AE_EVENT_PE_READY,
};

struct ae_event {
	double time;
	enum ae_eventtype type;
	int icid;
	int peid;
	union {
		void *ptr;
		size_t i;
	} data;
};


static void flush_sendinfo_list(struct ae_sendinfo_list *l);
static void b_level_priorities(struct ae_mapping *map, int maximum_parallelism);

/* compute communication-to-computation ratio (CCR) */
static double ae_ccr(struct ae_mapping *map)
{
	struct ae_task *task;
	int childid, parentid, temp;
	int nedges = 0;
	double commucost = 0.0;
	double compucost = 0.0;

	double mincommucost = 1E10;
	double avgcommucost = 0.0;
	double maxcommucost = 0.0;

	double mincompucost = 1E10;
	double avgcompucost = 0.0;
	double maxcompucost = 0.0;

	double taskcomp, taskcommu;

	double mintaskccr = 1E10;
	double avgtaskccr = 0.0;
	double maxtaskccr = 0.0;
	double taskccr;
	double ccr;

	AE_FOR_EACH_TASK(map, task, parentid) {

		taskcomp = ae_task_computation_time(map, parentid);
		compucost += taskcomp;

		if (taskcomp > 0 && taskcomp < mincompucost)
			mincompucost = taskcomp;

		if (taskcomp > maxcompucost)
			maxcompucost = taskcomp;

		taskcommu = 0.0;

		AE_FOR_EACH_CHILD(task, childid, temp) {
			nedges++;

			/* BROKEN: ICID == 0 ALWAYS. BUSES MUST BE SYMMETRIC */
			taskcommu = ae_communication_time(map->arch, 0, ae_task_send_amount(map, parentid, childid));

			commucost += taskcommu;
			taskcommu += taskcommu;

			if (taskcommu > 0 && taskcommu < mincommucost)
				mincommucost = taskcommu;

			if (taskcommu > maxcommucost)
				maxcommucost = taskcommu;
		}

		taskccr = taskcommu / taskcomp;

		if (taskccr > 0 && taskccr < mintaskccr)
			mintaskccr = taskccr;

		if (taskccr > maxtaskccr)
			maxtaskccr = taskccr;

		avgtaskccr += taskccr;
	}

	assert(nedges > 0);
	assert(commucost > 0.0);
	assert(compucost > 0.0);

	avgcommucost = commucost / nedges;
	avgcompucost = compucost / map->ntasks;
	ccr = avgcommucost / avgcompucost;
	avgtaskccr /= map->ntasks;

	printf("ccr: %.6lf\n", ccr);
	printf("min_avg_max_task_ccr: %.6f %.6f %.6f\n", mintaskccr, avgtaskccr, maxtaskccr);
	printf("min_avg_max_edge_communication_time: %.9f %.9f %.9f\n", mincommucost, avgcommucost, maxcommucost);
	printf("min_avg_max_task_computation_time: %.9f %.9f %.9f\n", mincompucost, avgcompucost, maxcompucost);

	return ccr;
}


static int ae_compare_event(void *a, void *b)
{
	struct ae_event *sa = (struct ae_event *) a;
	struct ae_event *sb = (struct ae_event *) b;
	/* extract max will return smallest time */
	if (sa->time < sb->time)
		return 1;
	if (sb->time < sa->time)
		return -1;
	return 0;
}

/*
 * This function is only called ONCE during program execution.
 * This is not called for each mapping that is scheduled.
 */
void ae_init_schedule(struct ae_mapping *map)
{
	struct ae_schedule *s;

	CALLOC_ARRAY(s, 1);
	map->schedule = s;

	CALLOC_ARRAY(s->pe_utilisations, map->arch->npes);
	CALLOC_ARRAY(s->ic_utilisations, map->arch->nics);

	map->appmodel->init_schedule(map);
}


static void flush_sendinfo_list(struct ae_sendinfo_list *l)
{
	l->n = 0;
}



/* partition each result into groups of interconnect transactions. each
   group consists of destination tasks that are on the same processing
   element. as a consequence, the partition is different for each mapping.
   notice that each tasks has 0 or more results.

   the complexity of this computation is O(E + P), where E is edges of the
   task graph, and P is the number of processing elements. watch out, because
   this is a tricky function, because it was optimized to be O(E + P).

   notice that memory is from a pre-allocated pool in map->schedule->resparts*/
static void send_id_generation(struct ae_mapping *map)
{
	int *space;
	int partoffset = 0;
	struct ae_schedule *s = map->schedule;

	struct ae_task *task;
	int taskid;
	int resind, j;
	int resultid;
	int iteration = 0;
	int dstid;
	int dst;
	int peid;
	struct ae_taskresult *result;
	struct {
		int iter;
		int dstnumb;
		int offs;
	} *pes;
	int *usedpes;
	int nusedpes;
	int accumulator;
	struct ae_sendinfo *sendinfo;

	CALLOC_ARRAY(pes, map->arch->npes);
	CALLOC_ARRAY(usedpes, map->arch->npes);

	AE_FOR_EACH_TASK(map, task, taskid) {

		/* loop through each result of task */
		for (resind = 0; resind < task->nresult; resind++) {

			resultid = RESULT_ID(map, taskid, resind);
			result = &task->result[resind];

			iteration++;
			nusedpes = 0;
			for (dstid = 0; dstid < result->ndst; dstid++) {
				dst = result->dst[dstid];
				peid = map->mappings[dst];
				/* hack. notice that pes[] table is not cleared between different
				   resultids (see the outer loop), but instead 'iteration' is used
				   as a marker to mark valid values in pes[] tables. */
				if (pes[peid].iter == iteration) {
					pes[peid].dstnumb++;
				} else {
					pes[peid].iter = iteration;
					pes[peid].dstnumb = 1;
					usedpes[nusedpes] = peid;
					nusedpes++;
				}
			}

			accumulator = 0;
			for (j = 0; j < nusedpes; j++) {
				peid = usedpes[j];
				pes[peid].offs = accumulator;
				accumulator += sizeof(struct ae_sendinfo) / sizeof(int) + pes[peid].dstnumb;
			}

			/* allocate: npes, {peid, ntasks, ntasksdone, tasks}, {peid, ntasks, ntasksdone, tasks}, ... */
			assert(accumulator == (sizeof(struct ae_sendinfo) / sizeof(int)) * nusedpes + result->ndst);

			if ((partoffset + accumulator) > s->respartsallocated) {
				s->respartsallocated = 2 * (partoffset + accumulator);
				REALLOC_ARRAY(s->resparts, s->respartsallocated);
			}

			s->resultpartition[resultid] = partoffset;
			space = &s->resparts[partoffset];
			partoffset += accumulator;

			for (j = 0; j < nusedpes; j++) {
				peid = usedpes[j];
				space[pes[peid].offs + 0] = nusedpes;	/* npartitions */
				space[pes[peid].offs + 1] = result->bytes;	/* byte amount */
				space[pes[peid].offs + 2] = peid;	/* dst pe */
				space[pes[peid].offs + 3] = pes[peid].dstnumb;	/* number of dst tasks */
				space[pes[peid].offs + 4] = 0;	/* reference count */

				/* verify that struck is packed and ordered right */
				sendinfo = (struct ae_sendinfo *) &space[pes[peid].offs];
				assert(sendinfo->npartitions == nusedpes);
				assert(sendinfo->bytes == result->bytes);
				assert(sendinfo->dstpeid == peid);
				assert(sendinfo->ndsttasks == pes[peid].dstnumb);
				assert(sendinfo->refcount == 0);

				pes[peid].offs += sizeof(struct ae_sendinfo) / sizeof(int);
			}

			for (dstid = 0; dstid < result->ndst; dstid++) {
				dst = result->dst[dstid];
				peid = map->mappings[dst];
				assert(pes[peid].offs < accumulator);
				space[pes[peid].offs++] = dst;
			}

			assert(((intptr_t) space) <= ((intptr_t) (s->resparts + s->respartsallocated)));
		}
	}

	free(pes);
	free(usedpes);
}

static void init_pes(struct ae_pe_state **pss, struct ae_mapping *map)
{
	int peid;

	MALLOC_ARRAY(*pss, map->arch->npes);

	for (peid = 0; peid < map->arch->npes; peid++)
		ae_pe_init_work_queue(&(*pss)[peid], map);
}

static void free_pes(struct ae_pe_state *pss, struct ae_mapping *map)
{
	int peid;

	for (peid = 0; peid < map->arch->npes; peid++)
		ae_pe_free_work_queue(&pss[peid]);
	free(pss);
}

static void init_utilisations(struct ae_schedule *s, struct ae_arch *arch)
{
	int i;
	for (i = 0; i < arch->npes; i++)
		s->pe_utilisations[i] = 0.0;

	for (i = 0; i < arch->nics; i++)
		s->ic_utilisations[i] = 0.0;

	s->arbs = 0;
	s->arbavgtime = 0.0;
	s->arbavginqueue = 0.0;
}

static void finalize_utilisations(struct ae_schedule *s, struct ae_arch *arch)
{
	int i;

	/* Get utilisation values for PEs and IC */
	for (i = 0; i < arch->npes; i++)
		s->pe_utilisations[i] /= s->schedule_length;

	for (i = 0; i < arch->nics; i++)
		s->ic_utilisations[i] /= s->schedule_length;

	if (s->arbs > 0) {
		s->arbavgtime /= s->arbs;
		s->arbavginqueue /= s->arbs;
	}
}

static struct ae_event new_event(struct ae_heap *eventheap)
{
	struct ae_event event;
	assert(eventheap->n > 0);
	ae_heap_extract_max(&event, eventheap);
	assert(event.type >= AE_EVENT_COMM_FIN && event.type <= AE_EVENT_PE_READY);
	return event;
}

static void queue_event(struct ae_heap *eventheap, double time,
			enum ae_eventtype type, int icid, int peid, void *data)
{
	struct ae_event event = {.time = time,
				 .type = type,
				 .icid = icid,
				 .peid = peid,
				 .data.ptr = data};
	ae_heap_insert(eventheap, &event);
}

static void comm_fin(struct ae_heap *eventheap,
		     double time, int icid, size_t i)
{
	struct ae_event event = {.time = time,
				 .type = AE_EVENT_COMM_FIN,
				 .icid = icid,
				 .peid = -1,
				 .data.i = i};
	ae_heap_insert(eventheap, &event);
}

static void ic_ready(struct ae_heap *eventheap,
		     double time, int icid, void *data)
{
	queue_event(eventheap, time, AE_EVENT_IC_READY, icid, -1, data);
}

static void comp_fin(struct ae_heap *eventheap,
		     double time, int peid, void *data)
{
	queue_event(eventheap, time, AE_EVENT_COMP_FIN, -1, peid, data);
}

static void pe_ready(struct ae_heap *eventheap,
		     double time, int peid, void *data)
{
	queue_event(eventheap, time, AE_EVENT_PE_READY, -1, peid, data);
}

static void task_ready(struct ae_pe_state *pss, int peid, double pri, 
		       int taskid, struct ae_mapping *map)
{
	struct ae_taskpri readytask = {.taskid = taskid};
	readytask.pri = (map->taskpriorities != NULL) ? map->taskpriorities[taskid] : pri;
	ae_heap_insert(&pss[peid].readyheap, &readytask);
}

static void task_and_pe_ready(struct ae_heap *eventheap,
			      struct ae_pe_state *pss,
			      double tasktime,
			      double petime,
			      int taskid,
			      struct ae_mapping *map)
{
	int peid = map->mappings[taskid];
	task_ready(pss, peid, tasktime, taskid, map);
	pe_ready(eventheap, petime, peid, NULL);
}

static int pick_ic(struct ae_ic_state *icstate, double curtime, int onlyfree)
{
	double minsendtime = 1E100;
	double sendtime;
	int minicid = -1;
	int icid;

	for (icid = 0; icid < icstate->nics; icid++) {
		if (onlyfree && icstate->ic[icid].status != 0)
			continue;
		sendtime = ae_ic_earliest_free_slot(icid, icstate, curtime);
		if (sendtime < minsendtime) {
			minsendtime = sendtime;
			minicid = icid;
		}
	}

	assert(minicid >= 0 || onlyfree);
	return minicid;
}

void ae_schedule_stg(struct ae_mapping *map)
{
	struct ae_schedule *s = map->schedule;
	struct ae_arch *arch = map->arch;
	struct ae_task *task;

	struct ae_taskpri readytask;

	struct ae_heap eventheap;
	struct ae_event event;

	struct ae_sendpri readysend;

	int childid, icid, peid, taskid;
	int *finishflag;
	int finished;
	double curtime;
	int state;
	int i;
	int resind, resdstind;

	struct ae_pe_state *pss;
	struct ae_ic_state icstate;

	int *rescount;

	int nsends;
	struct ae_sendinfo *sendinfo;

	int used_interconnect;
	int task_became_ready;

	int nevents = 0;

	double duration;

	ae_latency_costs(map);
	b_level_priorities(map, 0);

	send_id_generation(map);

	init_pes(&pss, map);
	ae_ic_init_work_queue(arch->nics, &icstate);
	ae_heap_init(&eventheap, ae_compare_event, sizeof(event));

	curtime = 0.0;

	AE_FOR_EACH_TASK(map, task, taskid) {
		if (task->nin == 0) {
			peid = map->mappings[taskid];
			task_ready(pss, peid, s->pri[taskid], taskid, map);
			pe_ready(&eventheap, curtime, peid, NULL);
		}
	}

	assert(eventheap.n > 0);

	CALLOC_ARRAY(rescount, map->ntasks);
	CALLOC_ARRAY(finishflag, map->ntasks);

	for (i = 0; i < map->ntasks; i++)
		flush_sendinfo_list(&s->tasksendinfos[i]);

	for (i = 0; i < map->ntresults; i++)
		s->result_refs[i] = 0;

	init_utilisations(s, arch);

	finished = 0;

	state = STATE_CHECK_EVENTS;

	while (state != STATE_FINISHED) {

		event = new_event(&eventheap);

		curtime = event.time;
		/* first process all communication events. the order is not necessary
		   but it can lead to a better schedule. a communication event might
		   make some important task ready to be runned before some other
		   ready task. */

		nevents++;

		switch (event.type) {
		case AE_EVENT_COMM_FIN:

			icid = event.icid;
			assert(icid >= 0 && icid < arch->nics);

			assert(icstate.ic[icid].status != 0);
			icstate.ic[icid].status = 0;

			sendinfo = icstate.ic[icid].sendinfo;

			assert(sendinfo->dstpeid >= 0 && sendinfo->dstpeid < arch->npes);
			assert(sendinfo->ndsttasks > 0);

			/* all the destination tasks are on the same pe */
			peid = sendinfo->dstpeid;

			task_became_ready = 0;

			for (resdstind = 0; resdstind < sendinfo->ndsttasks; resdstind++) {
				/* if destination child dependencies get satisfied, make it
				   ready for execution on a PE */
				childid = sendinfo->dsttasks[resdstind];
				assert(childid >= 0 && childid < map->ntasks);
				assert(map->mappings[childid] == peid);

				rescount[childid]++;
				if (rescount[childid] == map->tasks[childid]->ntresin) {
					task_ready(pss, peid, s->pri[childid], childid, map);
					task_became_ready = 1;
				}
			}

			if (task_became_ready)
				pe_ready(&eventheap, curtime, peid, NULL);

			if (icstate.sendqueue.n > 0)
				ic_ready(&eventheap, curtime, icid, NULL);

			break;

		case AE_EVENT_COMP_FIN:
			peid = event.peid;

			assert(pss[peid].busy);
			pss[peid].busy = 0;

			taskid = pss[peid].taskid;
			assert(taskid >= 0 && taskid < map->ntasks);

			for (i = 0; i < s->tasksendinfos[taskid].n; i++) {
				sendinfo = s->tasksendinfos[taskid].sendinfos[i];
				assert(sendinfo->refcount > 0);
				sendinfo->refcount--;
			}

			task = map->tasks[taskid];

			used_interconnect = 0;

			/* loop through each result of the task */
			for (resind = 0; resind < task->nresult; resind++) {

				int resultid = RESULT_ID(map, taskid, resind);
				sendinfo = RESULT_SEND_INFO(map, resultid);
				nsends = sendinfo->npartitions;
				assert(nsends >= 0);

				readysend.srctaskid = taskid;

				/* iterate through all result partitions. each partition contains
				   destination tasks that have the same processing element */
				while (nsends > 0) {
					/* sendinfo[0] = destination processing element id
					   sendinfo->ndsttasks = number of destination tasks
					   sendinfo[2] = number of destination tasks done (memory tracking)
					   sendinfo[3, ...] = destination task ids */
					readysend.sendinfo = sendinfo;

					assert(sendinfo->dstpeid >= 0 && sendinfo->dstpeid < arch->npes);
					assert(sendinfo->ndsttasks > 0);

					/* if destination tasks are on the same PE as the source task, then
					   send communication finish notices immediately */
					if (sendinfo->dstpeid == peid) {

						int bytes = sendinfo->bytes;
						assert(map->tresults[resultid]->bytes == bytes);

						for (resdstind = 0; resdstind < sendinfo->ndsttasks; resdstind++) {
							/* if destination child dependencies get satisfied, make it
							   ready for execution on a PE */
							childid = sendinfo->dsttasks[resdstind];
							assert(map->mappings[childid] == peid);
							rescount[childid]++;
							if (rescount[childid] == map->tasks[childid]->ntresin) {
								task_ready(pss, peid, s->pri[childid], childid, map);
							}
						}

					} else {

						/* sending to different pe */
						readysend.pri = 0.0;
						for (resdstind = 0; resdstind < sendinfo->ndsttasks; resdstind++) {
							childid = sendinfo->dsttasks[resdstind];
							assert(s->pri[childid] >= 0.0);
							if (readysend.pri < s->pri[childid])
								readysend.pri = s->pri[childid];
						}

						/* WARNING: ICID == 0. BUSES MUST BE SYMMETRIC. */
						readysend.pri += ae_communication_time(arch, 0, map->tresults[resultid]->bytes);
						readysend.resultid = resultid;

						ae_heap_insert(&icstate.sendqueue, &readysend);
						used_interconnect = 1;
					}

					/* skip to next result partition. hack hack. */
					sendinfo = (struct ae_sendinfo *) (((int *) sendinfo) + sizeof(struct ae_sendinfo) / sizeof(int) + sendinfo->ndsttasks);
					nsends--;
				}
			}

			if (used_interconnect) {
				/* HACK: icid == -1 is totally ugly */
				ic_ready(&eventheap, curtime, -1, NULL);
			}

			if (pss[peid].readyheap.n > 0)
				pe_ready(&eventheap, curtime, peid, NULL);

			if (finishflag[taskid] == 0) {
				finishflag[taskid] = 1;
				finished++;
				if (finished == map->ntasks) {
					state = STATE_FINISHED;
					for (i = 0; i < map->ntasks; i++)
						assert(finishflag[i] != 0);
				}
			}
			break;

		case AE_EVENT_IC_READY:

			icid = event.icid;
			if (icid == -1)
				icid = pick_ic(&icstate, curtime, 1);

			if (icid == -1 || icstate.ic[icid].status != 0)
				break; /* no free interconnect */

			if (icstate.sendqueue.n > 0) {
				int resultid;
				int bytes;

				ae_heap_extract_max(&readysend, &icstate.sendqueue);

				sendinfo = readysend.sendinfo;
				resultid = readysend.resultid;

				bytes = sendinfo->bytes;
				assert(bytes == map->tresults[resultid]->bytes);

				duration = ae_communication_time(arch, icid, bytes);

				s->ic_utilisations[icid] += duration;

				ae_ic_queue_work(icid, &icstate, curtime, duration, 1);

				comm_fin(&eventheap, ae_ic_earliest_free_slot(icid, &icstate, curtime), icid, 0);

				icstate.ic[icid].sendinfo = sendinfo;
				icstate.ic[icid].resultid = resultid;
			}
			break;

		case AE_EVENT_PE_READY:

			peid = event.peid;
			assert(peid >= 0 && peid < arch->npes);

			if (pss[peid].busy)
				break;

			if (pss[peid].readyheap.n > 0) {

				ae_heap_extract_max(&readytask, &pss[peid].readyheap);

				taskid = readytask.taskid;

				assert(peid == map->mappings[taskid]);

				duration = s->latencies[taskid] + ae_task_computation_time(map, taskid);

				s->pe_utilisations[peid] += duration;

				ae_pe_queue_work(&pss[peid], curtime, duration, taskid);

				comp_fin(&eventheap, ae_pe_earliest_free_slot(&pss[peid], curtime), peid, NULL);

				/* increase CPU result memory usage for each result that is computed
				   for 'readytask' */
				task = map->tasks[taskid];
				for (resind = 0; resind < task->nresult; resind++) {

					int resultid = RESULT_ID(map, taskid, resind);
					sendinfo = RESULT_SEND_INFO(map, resultid);

					assert(sendinfo->bytes == map->tresults[resultid]->bytes);

					/* task result reference counting.
					   result memory is freed after all partitions for this result
					   have been sent. note that this includes sending the result to
					   _this_ PE too. result_refs array is used to keep track of
					   partitions that have been sent. in practice these reference
					   counts are decreased on two places: 1. interconnect activity
					   ends, and 2. some task on this PE finishes. */
					s->result_refs[resultid] = sendinfo->npartitions;	/* number of partitions */
				}
			}
			break;

		default:
			ae_err("unknown event %d\n", state);
		}
	}

	s->schedule_length = curtime;

	finalize_utilisations(s, arch);

	free_pes(pss, map);

	ae_ic_free_work_queue(&icstate);
	ae_heap_free(&eventheap);
	free(rescount);
	free(finishflag);
}

static void all_processes_ready(struct ae_heap *eventheap,
				struct ae_pe_state *pss,
				struct ae_mapping *map)
{
	int taskid;
	int peid;
	struct ae_event event = {.time = 0.0,
				 .type = AE_EVENT_PE_READY};

	/* Make all processes ready */
	for (taskid = 0; taskid < map->ntasks; taskid++) {
		peid = map->mappings[taskid];
		task_ready(pss, peid, 0.0, taskid, map);
	}

	/* Generate PE_READY events for each PE */
	for (peid = 0; peid < map->arch->npes; peid++) {
		event.peid = peid;
		ae_heap_insert(eventheap, &event);
	}
}

static int get_ready_task(struct kpn_event *kpnevent,
			  struct ae_heap *readyheap,
			  struct kpn_state *kpnstate,
			  struct ae_mapping *map)
{
	struct ae_taskpri readytask;
	int taskid = -1;

	while (readyheap->n > 0) {
		ae_heap_extract_max(&readytask, readyheap);
		if (ae_kpn_execute(kpnevent, kpnstate, readytask.taskid, map)) {
			taskid = readytask.taskid;
			break;
		}
		/* Task is blocked, no need to put it back to readyheap */
	}

	return taskid;
}

void start_ic_arbitration(struct ae_ic_state *icstate,
			  struct ae_heap *eventheap,
			  double curtime, struct kpn_inst *kpninst,
			  struct ae_mapping *map)
{
	double starttime;
	int icid;
	int gen_ic_ready;
	struct vplist *transferqueue;
	struct transfer_unit *transferunit;

	/* Pick IC that allows earliest start time */
	icid = pick_ic(icstate, curtime, 0);

	transferqueue = icstate->ic[icid].transferqueue;

	/* Generate IC ready event if there are no on-going transfers */
	gen_ic_ready = vplist_is_empty(transferqueue);

	MALLOC_ARRAY(transferunit, 1);
	transferunit->issuetime = curtime;
	transferunit->inst = kpninst;

	if (vplist_append(transferqueue, transferunit))
		ae_err("No memory to append to instruction queue\n");

	if (gen_ic_ready) {
		starttime = ae_ic_earliest_free_slot(icid, icstate, curtime);
		ic_ready(eventheap, starttime, icid, NULL);
	}
}

void ae_schedule_kpn(struct ae_mapping *map)
{
	struct ae_schedule *s = map->schedule;
	struct ae_arch *arch = map->arch;

	struct ae_heap eventheap;
	struct ae_event event;
	struct ae_pe_state *pss;
	struct ae_ic_state icstate;
	struct kpn_state kpnstate;
	struct kpn_event kpnevent;
	struct kpn_inst *kpninst;
	struct transfer_unit *transferunit;

	double curtime;
	double duration;
	double starttime;
	double endtime;

	int taskid;
	int icid;
	int peid;
	size_t instindex;

	struct vplist *transferqueue;

	init_pes(&pss, map);
	ae_ic_init_work_queue(arch->nics, &icstate);
	ae_heap_init(&eventheap, ae_compare_event, sizeof(event));
	init_utilisations(s, arch);

	all_processes_ready(&eventheap, pss, map);

	ae_init_kpn_state(&kpnstate, map);

	while (1) {
		event = new_event(&eventheap);

		curtime = event.time;

		switch (event.type) {
		case AE_EVENT_COMM_FIN:
			instindex = event.data.i;
			icid = event.icid;

			transferqueue = icstate.ic[icid].transferqueue;
			transferunit = vplist_pop(transferqueue, instindex);
			kpninst = transferunit->inst;
			free(transferunit);
			transferunit = NULL;

			/* Retrigger IC ready if more insts in the queue */
			if (!vplist_is_empty(transferqueue))
				ic_ready(&eventheap, curtime, icid, NULL);

			if (ae_kpn_unblock(&kpnstate, kpninst))
				task_and_pe_ready(&eventheap, pss, curtime, curtime, kpninst->dst, map);
			break;

		case AE_EVENT_IC_READY:
			icid = event.icid;
			kpninst = ae_ic_arbitrate(&instindex, icid, curtime, &icstate, map);
			assert(kpninst != NULL);

			duration = ae_communication_time(arch, icid, kpninst->amount);
			s->ic_utilisations[icid] += duration;

			starttime = ae_ic_queue_work(icid, &icstate, curtime, duration, 0);
			assert(starttime == curtime);

			endtime = starttime + duration;

			comm_fin(&eventheap, endtime, icid, instindex);
			break;

		case AE_EVENT_COMP_FIN:
			kpnstate.ninstsleft--;
			if (kpnstate.ninstsleft == 0)
				goto finished;

			pss[event.peid].busy = 0;

			kpninst = event.data.ptr;
			taskid = pss[event.peid].taskid;

			if (kpninst->type == KPN_WRITE) {
				if (map->mappings[kpninst->src] == map->mappings[kpninst->dst]) {
					if (ae_kpn_unblock(&kpnstate, kpninst))

						task_ready(pss, map->mappings[kpninst->dst], curtime, kpninst->dst, map);
				} else {
					start_ic_arbitration(&icstate, &eventheap, curtime, kpninst, map);
				}
			}

			/*
			 * Schedule task for curtime, and
			 * schedule PE_READY for "lastendtime"
			 */
			task_and_pe_ready(&eventheap, pss, curtime, pss[event.peid].lastendtime, taskid, map);
			break;

		case AE_EVENT_PE_READY:
			peid = event.peid;
			assert(peid >= 0 && peid < arch->npes);

			if (pss[peid].busy)
				break;

			taskid = get_ready_task(&kpnevent, &pss[peid].readyheap, &kpnstate, map);
			if (taskid < 0)
				break; /* no ready tasks */

			assert(peid == map->mappings[taskid]);

			s->pe_utilisations[peid] += kpnevent.duration;

			ae_pe_queue_work(&pss[peid], curtime, kpnevent.duration, taskid);

			comp_fin(&eventheap, ae_pe_earliest_free_slot(&pss[peid], curtime), peid, kpnevent.inst);
			break;

		default:
			ae_err("Unknown kpn schedule event: %d\n", event.type);
		}
	}

finished:
	s->schedule_length = curtime;
	finalize_utilisations(s, arch);

	ae_free_kpn_state(&kpnstate, map);
	free_pes(pss, map);
	ae_ic_free_work_queue(&icstate);
	ae_heap_free(&eventheap);
}

static void b_level_priorities(struct ae_mapping *map, int maximum_parallelism)
{
	int i, childid, taskid, temp;
	struct ae_task **tasks = map->tasks;
	int ntasks = map->ntasks;
	double maximum;
	double pri, compcost;
	int peid;
	struct ae_schedule *s = map->schedule;

	for (taskid = 0; taskid < ntasks; taskid++)
		s->pri[taskid] = 0.0;

	for (i = 0; i < ntasks; i++) {
		taskid = s->tsort[i];
		peid = map->mappings[taskid];

		/* XXX: fix specific communication cost! they must be based on results, not
		   destination tasks. do not use AE_FOR_EACH_CHILD! */
		compcost = ae_task_computation_time(map, taskid);
		maximum = compcost;
		AE_FOR_EACH_CHILD(tasks[taskid], childid, temp) {
			pri = s->pri[childid];

			if (!maximum_parallelism)
				/* WARNING: ASSUME ICID == 0. BUSES MUST BE SYMMETRIC */
				pri += ae_specific_communication_time(map, 0, taskid, childid);

			pri += compcost;
			if (maximum < pri)
				maximum = pri;
		}

		s->pri[taskid] = maximum;
	}
}


static void per_task_statistics(double accrs[3], double bytes[3], struct ae_mapping *map)
{
	int childid, taskid, temp;
	int ntasks = map->ntasks;
	double nbytes;
	double nmin = 1E10, navg = 0.0, nmax = 0.0;
	double amin = 1E10, aavg = 0.0, amax = 0.0;
	double accr;

	for (taskid = 0; taskid < ntasks; taskid++) {

		nbytes = 0.0;

		AE_FOR_EACH_CHILD(map->tasks[taskid], childid, temp) {
			nbytes += ae_task_send_amount(map, taskid, childid);
		}

		/* Compute min, avg and max byte amounts */
		if (nbytes > 0 && nbytes < nmin)
			nmin = nbytes;

		if (nbytes > nmax)
			nmax = nbytes;

		navg += nbytes;

		/* Compute min, avg and max byte/s amounts */
		accr = nbytes / ae_task_computation_time(map, taskid);

		if (accr > 0.0 && accr < amin)
			amin = accr;

		if (accr > amax)
			amax = accr;

		aavg += accr;
	}

	bytes[0] = nmin;
	bytes[1] = navg / ntasks;
	bytes[2] = nmax;

	accrs[0] = amin;
	accrs[1] = aavg / ntasks;
	accrs[2] = amax;
}


void ae_stg_graph_stats(struct ae_mapping *map)
{
	int maxout, maxin;
	int avgout, avgin;
	int minout, minin;

	struct ae_task *task;
	int taskid;

	double critical_path;
	double compucost;

	double accr[3];
	double bytes[3];

	ae_ccr(map);

	printf("edges: %d\n", ae_task_edges(map));

	/* Compute the maximum and average number of input and output edges */
	avgout = avgin = 0;
	maxout = maxin = 0;
	minout = minin = map->ntasks;

	AE_FOR_EACH_TASK(map, task, taskid) {
		/* Output edges */
		if (task->nout > maxout)
			maxout = task->nout;

		if (task->nout > 0 && task->nout < minout)
			minout = task->nout;

		avgout += task->nout;

		/* Input edges */
		if (task->nin > maxin)
			maxin = task->nin;

		if (task->nin > 0 && task->nin < minin)
			minin = task->nin;

		avgin += task->nin;
	}

	printf("min_avg_max_out_edges: %d %f %d\n", minout, ((double) avgout) / map->ntasks, maxout);
	printf("min_avg_max_in_edges: %d %f %d\n", minin, ((double) avgin) / map->ntasks, maxin);

	per_task_statistics(accr, bytes, map);
	printf("min_avg_max_bytes: %f %f %f\n", bytes[0], bytes[1], bytes[2]);
	printf("min_avg_max_bytes/s: %f %f %f\n", accr[0], accr[1], accr[2]);

	b_level_priorities(map, 1);

	compucost = 0.0;
	critical_path = 0.0;

	for (taskid = 0; taskid < map->ntasks; taskid++) {
		double pri = map->schedule->pri[map->schedule->tsort[taskid]];

		if (pri > critical_path)
			critical_path = pri;

		compucost += ae_task_computation_time(map, taskid);
	}

	printf("total_computation: %.9lf\n", compucost);
	printf("critical_path: %.9lf\n", critical_path);
	printf("maximum_parallelism: %.9f\n", compucost / critical_path);
}
