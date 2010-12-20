/*
 * DCS task mapper
 * Copyright (C) 2004-2010 Tampere University of Technology
 *
 * The program was originally written by Heikki Orsila <heikki.orsila@iki.fi>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "pe.h"
#include "arexbasic.h"
#include "input.h"


void ae_add_pe_string(struct ae_arch *arch, FILE *f)
{
  struct ae_pe *pe;
  char *s;

  CALLOC_ARRAY(pe, 1);

  pe->id = arch->npes;

  pe->freq = 50000000;
  pe->send_latency = 0;
  pe->per_byte_send_cost = 0.0;
  pe->copy_latency = 0;
  pe->per_byte_copy_cost = 0.0;
  pe->performance_factor = 1.0;
  pe->area = 1E-6;

  while (1) {
    s = ae_get_word(f);

    if (strcmp(s, "freq") == 0) {
      pe->freq = ae_get_long_long(f);
      assert(pe->freq > 0);

    } else if (strcmp(s, "send_cost") == 0) {
      pe->send_latency = ae_get_int(f);
      assert(pe->send_latency >= 0);

      pe->per_byte_send_cost = ae_get_double(f);
      assert(pe->per_byte_send_cost >= 0.0);

    } else if (strcmp(s, "copy_cost") == 0) {
      pe->copy_latency = ae_get_int(f);
      assert(pe->copy_latency >= 0);

      pe->per_byte_copy_cost = ae_get_double(f);
      assert(pe->per_byte_copy_cost >= 0.0);

    } else if (strcmp(s, "performance_factor") == 0) {
      pe->performance_factor = ae_get_double(f);
      assert(pe->performance_factor > 0.0);

    } else if (strcmp(s, "area") == 0) {
      pe->area = ae_get_double(f);
      assert(pe->area > 0.0);

    } else if(strcmp(s, "end_processing_element") == 0) {
      free(s);
      break;

    } else {
      ae_err("Unknown variable in PE context: %s\n", s);
    }

    free(s);
  }

  ae_add_resource(&arch->npes, &arch->npesallocated, (void ***) &arch->pes, pe);
}


static int ae_compare_taskpri(void *a, void *b)
{
  struct ae_taskpri *sa = (struct ae_taskpri *) a;
  struct ae_taskpri *sb = (struct ae_taskpri *) b;
  if (sa->pri < sb->pri)
    return -1;
  if (sb->pri < sa->pri)
    return 1;
  return 0;
}


int ae_pe_copy_cost(int amount, struct ae_pe *pe)
{
	return pe->copy_latency + (int) ceil(pe->per_byte_copy_cost * amount);
}


int ae_pe_send_cost(int amount, struct ae_pe *pe)
{
	return pe->send_latency + (int) ceil(pe->per_byte_send_cost * amount);
}


double ae_pe_earliest_free_slot(struct ae_pe_state *ps, double curtime)
{
  assert(curtime >= 0.0);
  return curtime > ps->lastendtime ? curtime : ps->lastendtime;
}


void ae_pe_free_work_queue(struct ae_pe_state *ps)
{
  free(ps->taskrefcount);
  ps->taskrefcount = NULL;
  ae_heap_free(&ps->readyheap);
}


void ae_pe_init_work_queue(struct ae_pe_state *ps, struct ae_mapping *map)
{
  ps->busy = 0;
  ps->lastendtime = 0.0;
  ps->taskid = -1;
  CALLOC_ARRAY(ps->taskrefcount, map->ntasks);
  ae_heap_init(&ps->readyheap, ae_compare_taskpri, sizeof(struct ae_taskpri));
}


void ae_pe_queue_work(struct ae_pe_state *ps, double curtime, double duration,
		      int taskid)
{
  double starttime;
  assert(curtime >= 0.0);
  assert(duration > 0.0);

  starttime = curtime > ps->lastendtime ? curtime : ps->lastendtime;
  ps->lastendtime = starttime + duration;

  assert(ps->busy == 0);
  ps->busy = 1;
  ps->taskid = taskid;
}
