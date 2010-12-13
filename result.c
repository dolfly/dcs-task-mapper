/*
 *   DCS task mapper
 *   Copyright (C) 2004-2010 Tampere University of Technology (Heikki Orsila)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

#include "result.h"
#include "mapping.h"
#include "optimization.h"
#include "schedule.h"
#include "arexbasic.h"
#include "sa.h"
#include "task.h"
#include "arch.h"

#define NFRAMES 1024

struct output_frame {
  float objective;
  float time;
} __attribute__((packed));


char *sa_output_file = NULL;


void ae_init_result(struct ae_mapping *map)
{
  CALLOC_ARRAY(map->result, 1);
  gettimeofday(&map->result->start_time, NULL);
}


void ae_free_result(struct ae_mapping *map)
{
  free(map->result->time);
  free(map->result->objective);
}


static int ae_get_timestr(char *str, size_t max, struct timeval *tv)
{
  /* get ascii representation of current time */
  struct tm tm;
  int success = 0;
  if (localtime_r(&tv->tv_sec, &tm) != NULL) {
    if (strftime(str, max, "%F %T", &tm) == 0) {
      fprintf(stderr, "shit luck. strftime() failed.\n");
    } else {
      success = 1;
    }
  } else {
    fprintf(stderr, "shit luck. localtime_r() failed.\n");
  }
  if (success == 0 && max != 0)
    str[0] = 0;
  return success;
}


void ae_print_result(struct ae_mapping *map, struct ae_mapping *oldmap)
{
  int taskid, peid, icid;
  double gain, utilisation;
  struct rusage usage;
  double timepassed;
  struct ae_result *r;
  char *heuristics;
  int nchanged;
  char starttime[80];
  char endtime[80];
  time_t curtime;
  int schedulemax, maxrejects;
  double zeroprob;
  double statE, dynE;
  double area;

  r = map->result;
  
  if (getrusage(RUSAGE_SELF, &usage)) {
    fprintf(stderr, "shit luck. getrusage() failed.\n");
    usage.ru_utime.tv_sec = 0;
    usage.ru_utime.tv_usec = 0;
  }
  timepassed = ((double) usage.ru_utime.tv_sec) + 0.000001 * usage.ru_utime.tv_usec;

  ae_get_timestr(starttime, sizeof(starttime), &r->start_time);
  if (time(&curtime) != (time_t) -1) {
    ae_get_timestr(endtime, sizeof(endtime), & (struct timeval) {.tv_sec = curtime });
  }

  assert(r->initial > 0.0);
  assert(r->best > 0.0);

  gain = r->initial / r->best;

  printf("objective_function: %s\n", map->optimization->objective_name);
  printf("power_k: %e\n", map->optimization->power_k);

  printf("optimization_method: %s\n", map->optimization->method_name);
  if (strstr(map->optimization->method_name, "simulated_annealing") == NULL) {
    heuristics = "None";
    schedulemax = -1;
    maxrejects = -1;
    zeroprob = -1;
  } else {
    struct ae_sa_parameters *sap;
    sap = (struct ae_sa_parameters *) map->optimization->params;
    heuristics = sap->heuristics_name;
    schedulemax = sap->schedule_max;
    maxrejects = sap->max_rejects;
    zeroprob = sap->zero_transition_prob;
  }
  printf("sa_heuristics: %s\n", heuristics);
  printf("sa_schedule_max: %d\n", schedulemax);
  printf("sa_max_rejects: %d\n", maxrejects);
  printf("sa_zero_transition_prob: %e\n", zeroprob);

  printf("ntasks: %d\n", map->ntasks);

  if (map->appmodel->graph_stats)
    map->appmodel->graph_stats(map);

  printf("pes: %d\n", map->arch->npes);
  printf("pe_utilisations: ");
  utilisation = 0.0;
  for (peid = 0; peid < map->arch->npes; peid++) {
    printf("%.3f ", map->schedule->pe_utilisations[peid]);

    utilisation += map->schedule->pe_utilisations[peid];
  }
  printf("\ntotal_pe_utilisation: %.3f\n", utilisation / map->arch->npes);

  printf("ic_utilisations: ");
  utilisation = 0.0;
  for (icid = 0; icid < map->arch->nics; icid++) {
    printf("%.3f ", map->schedule->ic_utilisations[icid]);

    utilisation += map->schedule->ic_utilisations[icid];
  }
  printf("\ntotal_ic_utilisation: %.3f\n", utilisation / map->arch->nics);

  printf("mapping_list %d ", map->ntasks);
  nchanged = 0;
  for (taskid = 0; taskid < map->ntasks; taskid++) {
    printf("map %d %d ", taskid, map->mappings[taskid]);
    if (map->mappings[taskid] != oldmap->mappings[taskid])
      nchanged++;
  }
  printf("\n");
  printf("changed_mappings: %d\n", nchanged);

  if (map->taskpriorities) {
    printf("task_priorities: ");
    for (taskid = 0; taskid < map->ntasks; taskid++)
      printf("%f ", map->taskpriorities[taskid]);
    printf("\n");
  }

  ae_print_mapping_balance(stdout, map);
  printf("data_file: %s\n", sa_output_file ? sa_output_file : "");
  printf("initial_objective: %.9lf\n", r->initial);
  printf("initial_time: %.9lf\n", r->initial_time);
  printf("best_objective: %.9lf\n", r->best);
  printf("best_time: %.9lf\n", r->best_time);
  printf("gain: %.3lf\n", gain);
  printf("time_gain: %.3lf\n", r->initial_time / r->best_time);

  ae_energy(&area, &statE, &dynE, map);
  printf("static_energy: %e\n", statE);
  printf("dynamic_energy: %e\n", dynE);
  printf("static_energy_proportion: %.3f\n", statE / (statE + dynE));
  printf("total_energy: %e\n", statE + dynE);
  printf("area: %e\n", area);

  printf("evaluations: %lld\n", r->evals);
  printf("optimization_time: %.6lf\n", timepassed);
  printf("optimization_started: %s\n", starttime);
  printf("optimization_ended: %s\n", endtime);

  /* write schedule iteration statistics to a file */
  if (sa_output_file != NULL) {
    size_t ret;
    FILE *out;

    struct output_frame frames[NFRAMES];
    long long left, n, i;

    if ((out = fopen(sa_output_file, "w")) == NULL)
      ae_err("can not write to output file: %s\n", sa_output_file);

    left = r->evals;
    while (left > 0) {
      n = (left >= NFRAMES) ? NFRAMES : left;
      for (i = 0; i < n; i++) {
	frames[i].objective = r->objective[i];
	frames[i].time = r->time[i];
      }
      i = 0;
      while (i < n) {
	ret = fwrite(&frames[i], sizeof(frames[0]), n - i, out);
	if (ret == 0)
	  ae_err("write error to output file: %s\n", strerror(errno));
	i += ret;
      }
      left -= n;
    }
    fclose(out);
  }
}
