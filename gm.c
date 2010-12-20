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
#include <assert.h>
#include <math.h>

#include "gm.h"
#include "mapping.h"
#include "arexbasic.h"
#include "optimization.h"
#include "result.h"


static struct ae_mapping *ae_gm_round(struct ae_mapping *S0, int round);
static struct ae_mapping *ae_gm2_round(struct ae_mapping *S0);


struct ae_mapping *ae_gm(struct ae_mapping *S0, int randomize)
{
  struct ae_mapping *S;
  struct ae_mapping *S_best;
  struct ae_mapping *S_new;
  double bestcost, newcost;
  int round = 0;

  S = ae_fork_mapping(S0);
  S_best = ae_fork_mapping(S0);
  bestcost = S->optimization->objective(S_best);

  S0 = NULL;

  if (randomize)
    ae_randomize_mapping(S);

  while (1) {
    fprintf(stderr, "gm round %d\n", round++);
    fprintf(stderr, "best cost: %.9lf\n", bestcost);
    S_new = ae_gm_round(S, round);
    newcost = S->optimization->objective(S_new);
    if (cost_diff(bestcost, newcost) >= 0) {
      ae_free_mapping(S_new);
      break;
    }
    bestcost = newcost;
    ae_copy_mapping(S_best, S_new);
    ae_copy_mapping(S, S_new);
    ae_free_mapping(S_new);
  }
  ae_free_mapping(S);
  return S_best;
}


struct ae_mapping *ae_gm2(struct ae_mapping *S0)
{
  struct ae_mapping *S, *Snew;
  double initialcost, newcost;

  S = ae_fork_mapping(S0);

  while (1) {

    Snew = ae_gm(S, 0);

    ae_free_mapping(S);

    S = Snew;

    initialcost = S->optimization->objective(S);

    S = ae_gm2_round(S);

    newcost = S->optimization->objective(S);

    fprintf(stderr, "gm2 extra round gain: %f\n", initialcost / newcost);

    if (newcost >= initialcost)
      break;
  }

  return S;
}


static struct ae_mapping *ae_gm2_round(struct ae_mapping *S)
{
  int t1, t2, p1, p2, oldp1, oldp2;
  double bestcost, newcost;
  int best_t1 = -1, best_p1 = -1, best_t2 = -1, best_p2 = -1;

  bestcost = S->optimization->objective(S);

  for (t1 = 0; t1 < S->ntasks; t1++) {

    if (S->isstatic[t1] != 0)
      continue;

    oldp1 = S->mappings[t1];

    for (p1 = 0; p1 < S->arch->npes; p1++) {
      if (p1 == oldp1)
	continue;

      fprintf(stderr, "accepted_objective: %.9lf\n", bestcost);

      S->mappings[t1] = p1;

      for (t2 = 0; t2 < S->ntasks; t2++) {

	if (S->isstatic[t2] != 0 || t1 == t2)
	  continue;

	oldp2 = S->mappings[t2];

	for (p2 = 0; p2 < S->arch->npes; p2++) {

	  if (p2 == oldp2)
	    continue;

	  S->mappings[t2] = p2;

	  newcost = S->optimization->objective(S);
	  if (cost_diff(bestcost, newcost) < 0) {
	    bestcost = newcost;
	    best_t1 = t1;
	    best_p1 = p1;
	    best_t2 = t2;
	    best_p2 = p2;
	    printf("best_gm_cost_so_far: %lld %.9f\n", S->result->evals, bestcost);
	    fflush(stdout);
	  }
	}

	S->mappings[t2] = oldp2;
      }
    }

    S->mappings[t1] = oldp1;
  }

  if (best_t1 != -1) {
    S->mappings[best_t1] = best_p1;
    S->mappings[best_t2] = best_p2;
  }

  return S;
}


static struct ae_mapping *ae_gm_round(struct ae_mapping *S0, int round)
{
  struct ae_mapping *S;
  double bestcost, newcost;
  int bestpe, oldpe, pe;
  int *moved;
  int i;
  int besttask, taskid;

  S = ae_fork_mapping(S0);
  bestcost = S->optimization->objective(S);

  CALLOC_ARRAY(moved, S->ntasks);

  for (i = 0; i < S->ntasks; i++) {
    besttask = -1;
    bestpe = -1;
    for (taskid = 0; taskid < S->ntasks; taskid++) {
      if (S->isstatic[taskid] != 0 || moved[taskid] != 0)
	continue;
      oldpe = S->mappings[taskid];
      for (pe = 0; pe < S->arch->npes; pe++) {
	if (pe == oldpe)
	  continue;
	fprintf(stderr, "accepted_objective: %.9lf\n", bestcost);
	S->mappings[taskid] = pe;
	newcost = S->optimization->objective(S);
	if (cost_diff(bestcost, newcost) < 0) {
	  bestcost = newcost;
	  besttask = taskid;
	  bestpe = pe;
	  printf("best_gm_cost_so_far: %d %lld %.9f\n", round, S->result->evals, bestcost);
	  fflush(stdout);
	}
      }
      S->mappings[taskid] = oldpe;
    }
    if (besttask < 0)
      break;
    moved[besttask] = 1;
    S->mappings[besttask] = bestpe;
    fprintf(stderr, "subround cost: %.9lf\n", bestcost);
  }
  printf("best_gm_cost_so_far: %d %lld %.9f\n", round, S->result->evals, bestcost);
  fflush(stdout);
  free(moved);
  return S;
}
