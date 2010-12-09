#ifndef _AE_AREXTYPES_H_
#define _AE_AREXTYPES_H_

#include <stdint.h>

struct ae_taskresult {
	int bytes;
	int ndst;
	int *dst;
};

struct ae_task {
	int id;
	int nin;
	int *in;
	int nout;
	int *out;
	int *outbytes;
	int ntresin;
	int nresult;
	struct ae_taskresult *result;
	double weight;
};

enum kpn_inst_type {
	KPN_COMPUTE,
	KPN_READ,
	KPN_WRITE,
};

struct kpn_inst {
	enum kpn_inst_type type;
	int src;
	int dst;
	unsigned int amount;
};

struct kpn_process {
	int id;
	unsigned int ninsts;
	struct kpn_inst *insts;
};

struct ae_pe {
	int id;
	long long freq;

	int send_latency;
	double per_byte_send_cost;

	int copy_latency;
	double per_byte_copy_cost;

	double performance_factor;

	double area;

	int ic_initial_priority;
};

enum ic_arbitration {
	IC_FIFO_ARBITRATION = 0,
	IC_LIFO_ARBITRATION,     /* brain-damaged arbitration policy */
	IC_RANDOM_ARBITRATION,
	IC_PRIORITY_ARBITRATION,
};

struct ae_interconnect {
	int id;
	long long freq;
	double area;
	int width;
	int latency;
	enum ic_arbitration policy;
};

struct ae_arch {
	int npes;
	int npesallocated;
	struct ae_pe **pes;
	int nics;
	int nicsallocated;
	struct ae_interconnect **ics;
};

struct ae_optimization;
struct ae_schedule;
struct ae_result;
struct ae_appmodel;

struct ae_mapping {
	struct ae_arch *arch;
	int ntasks;
	int ntasksallocated;

	/* ntasks amount of mappings */
	int *mappings;
	int *isstatic;

	/* optional task priorities. Can be NULL. */
	double *taskpriorities;

	/* IC priorities for PEs */
	int *icpriorities;

	struct ae_schedule *schedule;
	struct ae_optimization *optimization;
	struct ae_result *result;

	struct ae_appmodel *appmodel;

	/* STG specific data follows */

	/* ntasks amount of tasks */
	struct ae_task **tasks;

	/* numbering for task results. map task to result id offset */
	int *tresultoffsets;
	int ntresults;

	/* map result number to task id */
	int *tresultinv;
	struct ae_taskresult **tresults;

	/* KPN specific data follows */

	/* ntasks amount of processes */
	struct kpn_process **processes;
};

struct ae_appmodel {
	const char *name;
	void (*init_schedule)(struct ae_mapping *map);
	void (*graph_stats)(struct ae_mapping *map);
	void (*schedule)(struct ae_mapping *map);
};

#endif
