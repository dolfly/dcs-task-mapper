#ifndef _AE_SCHEDULE_H_
#define _AE_SCHEDULE_H_

#include "arextypes.h"
#include "datastructures.h"

struct ae_tasklist {
	int n;
	int allocated;
	int *tasks;
};

/* order of the structure members have to be exact in memory. and they
   must be packed too. */
struct ae_sendinfo {
	int npartitions;
	int bytes;
	int dstpeid;
	int ndsttasks;
	int refcount;
	int dsttasks[];
} __attribute__((packed));

struct ae_sendinfo_list {
	int n;
	int allocated;
	struct ae_sendinfo **sendinfos;
};

struct ae_schedule {
	int *tsort;

	double *pri;
	double *latencies;

	int *resultpartition;

	int *resparts;
	int respartsallocated;

	struct ae_sendinfo_list *tasksendinfos;
	/* result references for output memory calculation */
	int *result_refs; 

	/*
	 * Following variables are later used as results. They are generated
	 * when schedule() is called.
	 */
	double schedule_length;
	double *pe_utilisations;
	double *ic_utilisations;

	int64_t arbs;
	double arbavginqueue;
	double arbavgtime;
};


void ae_init_schedule(struct ae_mapping *map);
void ae_stg_graph_stats(struct ae_mapping *map);
void ae_schedule_stg(struct ae_mapping *map);
void ae_schedule_kpn(struct ae_mapping *map);

#endif
