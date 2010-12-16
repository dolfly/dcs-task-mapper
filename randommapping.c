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
#include <math.h>

#include "randommapping.h"
#include "mapping.h"
#include "arexbasic.h"
#include "optimization.h"
#include "input.h"
#include "result.h"


struct ae_mapping *ae_random_mapping(struct ae_mapping *map, double initial)
{
  struct ae_mapping *newmap;
  struct ae_mapping *bestmap;
  double newcost, bestcost;
  int iteration, maxiteration;
  int ntasks = map->ntasks;
  int npes = map->arch->npes;
  struct ae_random_parameters *p;
  int print_progress;

  newmap = ae_fork_mapping(map);
  bestmap = ae_fork_mapping(map);
  bestcost = initial;

  p = map->optimization->params;
  if (p->max_iterations >= 0) {
    maxiteration = p->max_iterations;
  } else {
    maxiteration = p->constant * pow(ntasks, p->task_exp) * pow(npes, p->pe_exp);
    fprintf(stderr, "ae random constant: %lf\n", p->constant);
    fprintf(stderr, "ae random task_exp: %lf\n", p->task_exp);
    fprintf(stderr, "ae random pe_exp: %lf\n", p->pe_exp);
  }
  fprintf(stderr, "ae random max_iteration: %d\n", maxiteration);

  for (iteration = 0; iteration < maxiteration; iteration++) {

    fprintf(stderr, "accepted_objective: %.9lf\n", bestcost);

    ae_randomize_mapping(newmap);

    newcost = map->optimization->objective(newmap);

    if (cost_diff(bestcost, newcost) < 0) {
      bestcost = newcost;
      ae_copy_mapping(bestmap, newmap);
      print_progress = 1;
    } else {
      print_progress = (map->result->evals % 1000) == 0 ? 1 : 0;
    }

    if (print_progress) {
      printf("best_random_cost_so_far: %lld %.9f\n", map->result->evals, bestcost);
      fflush(stdout);
    }
  }
  ae_free_mapping(newmap);
  return bestmap;
}

/* format of random mapping parameters is:

       max_iterations   {int}
       multiplier       {float}
       task_exponent    {float}
       pe_exponent      {float}

   If max_iterations >= 0, then that number determines amount of iterations.
   Otherwise (max_iterations < 0) the iteration amount is determined by
   formula:
       constant * ntasks^{task_exp} * npes^{pe_exp}
*/
struct ae_random_parameters *ae_random_read_parameters(FILE *f)
{
  struct ae_random_parameters *p;
  if ((p = calloc(1, sizeof(*p))) == NULL)
    ae_err("no memory for random parameters\n");
  ae_match_word("max_iterations", f);
  p->max_iterations = ae_get_int(f);
  fprintf(stderr, "random max_iterations: %d\n", p->max_iterations);
  ae_match_word("multiplier", f);
  p->constant = ae_get_double(f);
  ae_match_word("task_exponent", f);
  p->task_exp = ae_get_double(f);
  ae_match_word("pe_exponent", f);
  p->pe_exp = ae_get_double(f);
  return p;
}
