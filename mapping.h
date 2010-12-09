#ifndef _AE_MAPPING_H_
#define _AE_MAPPING_H_

#include <stdio.h>

#include "arextypes.h"

double ae_communication_time(struct ae_arch *arch, int icid, int amount);
double ae_pe_computation_time(struct ae_pe *pe, int operations);
double ae_task_computation_time(struct ae_mapping *map, int taskid);
void ae_randomize_n_task_mappings(struct ae_mapping *m, int n);
void ae_copy_mapping(struct ae_mapping *newmap, struct ae_mapping *map);
struct ae_mapping *ae_create_mapping(void);
struct ae_mapping *ae_fork_mapping(struct ae_mapping *map);
void ae_free_mapping(struct ae_mapping *map);
void ae_initialize_task_priorities(struct ae_mapping *map);
void ae_latency_costs(struct ae_mapping *map);
void ae_print_mapping(struct ae_mapping *map);
void ae_print_mapping_balance(FILE *f, struct ae_mapping *map);
void ae_randomize_mapping(struct ae_mapping *map);
void ae_randomize_task_priorities(struct ae_mapping *map);
int ae_set_mapping(struct ae_mapping *map, int tid, int peid);
void ae_set_task_priority(struct ae_mapping *map, int tid, double pri);
double ae_specific_communication_time(struct ae_mapping *map, int icid, int srctask, int dsttask);
double ae_total_mappings(struct ae_mapping *map);
double ae_total_schedules(struct ae_mapping *map);
void ae_zero_mapping(struct ae_mapping *map);

#define AE_FOR_EACH_TASK(map, task, taskid) for ((taskid) = 0; (taskid) < (map)->ntasks && ((task) = (map)->tasks[(taskid)]) != NULL; (taskid)++)

#endif
