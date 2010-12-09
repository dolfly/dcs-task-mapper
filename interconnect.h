#ifndef _AE_IC_H_
#define _AE_IC_H_

#include <stdio.h>

#include "arextypes.h"
#include "datastructures.h"
#include "schedule.h"
#include "vplist.h"

struct transfer_unit {
	double issuetime;
	struct kpn_inst *inst;
};

struct ae_single_ic_state {
	int status;
	double lastendtime;
	struct ae_sendinfo *sendinfo;
	int resultid;
	struct vplist *transferqueue;
};

struct ae_ic_state {
	int nics;
	struct ae_single_ic_state *ic;
	struct ae_heap sendqueue;
};

struct ae_sendpri {
	double pri;
	int srctaskid;
	struct ae_sendinfo *sendinfo; /* peid, ndsts, dst1, dst2, ... */
	int resultid;
};

void ae_add_ic_string(struct ae_arch *arch, FILE *f);
struct kpn_inst *ae_ic_arbitrate(size_t *instindex, int icid, double curtime, struct ae_ic_state *icstate, struct ae_mapping *map);
double ae_ic_earliest_free_slot(int icid,
				struct ae_ic_state *icstate,
				double curtime);
void ae_ic_free_work_queue(struct ae_ic_state *icstate);
void ae_ic_init_work_queue(int nics, struct ae_ic_state *icstate);
double ae_ic_queue_work(int icid, struct ae_ic_state *icstate, double curtime, double duration, int busycheck);

#endif
