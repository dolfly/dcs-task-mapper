#ifndef _AE_KPN_H_
#define _AE_KPN_H_

#include <stdio.h>
#include "arextypes.h"
#include "intarray.h"
#include "interconnect.h"
#include "sa.h"

struct kpn_event {
	double duration;
	struct kpn_inst *inst;
};

struct kpn_process_state {
	/*
	 * Process is completed when programcounter is larger than the number
	 * of instructions.
	 */
	int programcounter;

	int blocked;

	struct intarray sources;
	struct intarray **fifos;
};

struct kpn_state {
	/* Total number of instructions left. When 0, terminate. */
	int ninstsleft;

	/* Process states */
	struct kpn_process_state *pstates;
};

void ae_free_kpn_state(struct kpn_state *kpnstate, struct ae_mapping *map);
void ae_init_kpn_schedule(struct ae_mapping *map);
void ae_init_kpn_state(struct kpn_state *kpnstate, struct ae_mapping *map);
void ae_kpn_autotemp(struct ae_sa_parameters *params, double minperf, double maxperf, double k, struct ae_mapping *map);
int ae_kpn_execute(struct kpn_event *event, struct kpn_state *kpnstate, int taskid, struct ae_mapping *map);
int ae_kpn_unblock(struct kpn_state *kpnstate, const struct kpn_inst *inst);
void ae_read_kpn(struct ae_mapping *map, FILE *f);

#endif
