#ifndef _AE_TASK_H_
#define _AE_TASK_H_

#include <stdio.h>
#include "arextypes.h"
#include "schedule.h"
#include "sa.h"

void ae_read_stg(struct ae_mapping *map, FILE *f);
int ae_task_edges(struct ae_mapping *map);
int ae_task_send_amount(struct ae_mapping *map, int srctaskid, int dsttask);
void ae_topological_sort(int *tsort, struct ae_mapping *map);
void ae_stg_sa_autotemp(struct ae_sa_parameters *params, double minperf, double maxperf, double k, struct ae_mapping *map);

#define AE_FOR_EACH_CHILD(task, childid, temp) for ((temp) = 0; (temp) < (task)->nout && ((childid) = (task)->out[(temp)]) != -1 ;(temp)++)

#define AE_FOR_EACH_PARENT(task, parentid, temp) for ((temp) = 0; (temp) < (task)->nin && ((parentid) = (task)->in[(temp)]) != -1 ;(temp)++)

#endif
