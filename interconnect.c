#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "interconnect.h"
#include "arextypes.h"
#include "arexbasic.h"
#include "input.h"

enum ic_arbitration decode_arbitration_policy(const char *policy)
{
	struct {
		char *policyname;
		enum ic_arbitration policy;
	} p[] = {{"fifo", IC_FIFO_ARBITRATION},
		 {"lifo", IC_LIFO_ARBITRATION},
		 {"priority", IC_PRIORITY_ARBITRATION},
		 {"random", IC_RANDOM_ARBITRATION},
		 {NULL, 0}};
	int i;
	for (i = 0; p[i].policyname != NULL; i++) {
		if (!strcmp(p[i].policyname, policy))
			return p[i].policy;
	}
	ae_err("Unknown arbitration policy: %s\n", policy);
	return IC_FIFO_ARBITRATION;
}

void ae_add_ic_string(struct ae_arch *arch, FILE *f)
{
	long long freq;
	int width;
	int lat;
	int ret;
	double area;
	struct ae_interconnect *ic;
	char *policy;
	char *par;

	ret = fscanf(f, "%lld %lf %d %d", &freq, &area, &width, &lat);
	if (ret != 4)
		ae_err("ae_get_ic_string: no input or too few parameters\n");

	if ((ic = calloc(1, sizeof(*ic))) == NULL)
		ae_err("ae_get_ic_string: no memory\n");

	assert(freq > 0);
	assert(width > 0 && width <= 1024);
	assert(area > 0.0);

	ic->id = arch->nics;
	ic->area = area;
	ic->freq = freq;
	ic->width = width;
	ic->latency = lat;

	while (1) {
		par = ae_get_word(f);
		if (!strcmp(par, "end_interconnect")) {
			free(par);
			par = NULL;
			break;
		}
		if (!strcmp(par, "arbitration")) {
			policy = ae_get_word(f);
			ic->policy = decode_arbitration_policy(policy);
			free(policy);
			policy = NULL;
		} else {
			ae_err("Unknown interconnect parameter: %s\n", par);
		}
		free(par);
		par = NULL;
	}

	if (ae_config.arbitration_policy != NULL)
		ic->policy = decode_arbitration_policy(ae_config.arbitration_policy);

	ae_add_resource(&arch->nics, &arch->nicsallocated, (void ***) &arch->ics, ic);
}


static int ae_compare_sendpri(void *a, void *b)
{
	struct ae_sendpri *sa = (struct ae_sendpri *) a;
	struct ae_sendpri *sb = (struct ae_sendpri *) b;
	if (sa->pri < sb->pri)
		return -1;
	if (sb->pri < sa->pri)
		return 1;
	return 0;
}

static size_t priority_arbitration(struct vplist *transferqueue,
				   struct ae_mapping *map)
{
	struct vplist *node;
	struct transfer_unit *transferunit;
	size_t index = 0;
	size_t maxindex = 0;
	int maxpriority = -1;
	int peid;
	int priority;

	VPLIST_FOR_EACH(node, transferqueue) {
		transferunit = node->item;
		peid = map->mappings[transferunit->inst->src];
		priority = map->icpriorities[peid];
		if (priority > maxpriority) {
			maxpriority = priority;
			maxindex = index;
		}
		index++;
	}

	return maxindex;
}

struct kpn_inst *ae_ic_arbitrate(size_t *instindex, int icid, double curtime,
				 struct ae_ic_state *icstate,
				 struct ae_mapping *map)
{
	struct vplist *transferqueue = icstate->ic[icid].transferqueue;
	struct ae_interconnect *ic = map->arch->ics[icid];
	struct transfer_unit *transferunit;
	size_t len;

	len = vplist_len(transferqueue);

	switch (ic->policy) {
	case IC_FIFO_ARBITRATION:
		*instindex = 0;
		break;
	case IC_LIFO_ARBITRATION:
		*instindex = len - 1;
		break;
	case IC_PRIORITY_ARBITRATION:
		*instindex = priority_arbitration(transferqueue, map);
		break;
	case IC_RANDOM_ARBITRATION:
		*instindex = ae_randi(0, len);
		break;
	default:
		ae_err("Invalid IC arbitration policy\n");
	}

	transferunit = vplist_get(transferqueue, *instindex);

	map->schedule->arbs += 1;
	map->schedule->arbavginqueue += len;
	map->schedule->arbavgtime += (curtime - transferunit->issuetime);

	return transferunit->inst;
}

double ae_ic_earliest_free_slot(int icid,
				struct ae_ic_state *icstate,
				double curtime)
{
	struct ae_single_ic_state *sicstate;

	assert(curtime >= 0.0);

	sicstate = &icstate->ic[icid];

	return (curtime > sicstate->lastendtime) ? curtime : sicstate->lastendtime;
}


void ae_ic_free_work_queue(struct ae_ic_state *icstate)
{
	int i;

	for (i = 0; i < icstate->nics; i++) {
		icstate->ic[i].sendinfo = NULL;
		vplist_free(icstate->ic[i].transferqueue);
		icstate->ic[i].transferqueue = NULL;
	}

	free(icstate->ic);
	icstate->ic = NULL;

	ae_heap_free(&icstate->sendqueue);
}


void ae_ic_init_work_queue(int nics, struct ae_ic_state *icstate)
{
	int i;

	assert(nics > 0);

	icstate->nics = nics;

	MALLOC_ARRAY(icstate->ic, nics);

	for (i = 0; i < nics; i++) {
		icstate->ic[i].lastendtime = 0.0;
		icstate->ic[i].sendinfo = NULL;
		icstate->ic[i].resultid = -1;
		icstate->ic[i].status = 0;
		icstate->ic[i].transferqueue = vplist_create();
		assert(icstate->ic[i].transferqueue != NULL);
	}

	ae_heap_init(&icstate->sendqueue, ae_compare_sendpri, sizeof(struct ae_sendpri));
}


double ae_ic_queue_work(int icid, struct ae_ic_state *icstate,
			double curtime, double duration, int busycheck)
{
	double starttime;
	struct ae_single_ic_state *sicstate;

	assert(curtime >= 0.0);
	assert(duration > 0.0);
	assert(icid >= 0 && icid < icstate->nics);

	sicstate = &icstate->ic[icid];

	starttime = curtime > sicstate->lastendtime ? curtime : sicstate->lastendtime;
	sicstate->lastendtime = starttime + duration;

	assert(!busycheck || sicstate->status == 0);

	sicstate->status = 1;

	return starttime;
}
