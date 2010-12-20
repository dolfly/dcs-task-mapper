#ifndef _AE_PE_H_
#define _AE_PE_H_

#include <stdio.h>

#include "arextypes.h"
#include "datastructures.h"


struct ae_pe_state {
	int busy;
	double lastendtime;
	int taskid;
	int *taskrefcount;
	struct ae_heap readyheap;
};


struct ae_taskpri {
	double pri;
	int taskid;
};


void ae_add_pe_string(struct ae_arch *arch, FILE *f);
int ae_pe_copy_cost(int amount, struct ae_pe *pe);
int ae_pe_send_cost(int amount, struct ae_pe *pe);
double ae_pe_earliest_free_slot(struct ae_pe_state *ps, double curtime);
void ae_pe_free_work_queue(struct ae_pe_state *ps);
void ae_pe_init_work_queue(struct ae_pe_state *ps, struct ae_mapping *map);
void ae_pe_queue_work(struct ae_pe_state *ps, double curtime, double duration, int taskid);

#endif
