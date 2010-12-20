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
#include <string.h>

#include "arexbasic.h"
#include "mapping.h"
#include "sa.h"
#include "input.h"
#include "mappingheuristics.h"
#include "result.h"
#include "task.h"
#include "kpn.h"

/* Simulated Annealing algorithm.

   Original papers about simulated annealing:

    @article{ kirkpatrick83optimization,
       author = "S. Kirkpatrick and C. D. Gelatt and M. P. Vecchi",
       title = "Optimization by Simulated Annealing",
       journal = "Science, Number 4598, 13 May 1983",
       volume = "220, 4598",
       pages = "671--680",
       year = "1983",
       url = "citeseer.nj.nec.com/kirkpatrick83optimization.html" }
   and
    V. Cerny, A thermodynamical approach to the traveling salesman problem:
    an efficient simulation algorithm,
    J. Optimization Theory and Appl. 45, 41-51.

 Parameters:

 S0          is the initial state of the system
 T           the initial (and current) temperature of the system
 objective   the function to be optimized (finding minimum)
 move        maps current state and 'temperature' into a next state
 acceptor    is the probability of accepting a state change of 'difference'
             and 'temperature'
 max_rejects is the maximum number of consecutive moves that are rejected.
 schedule    returns temperature a new temperature
 schedule_max is the number of mapping iterations per temperature level.
 ref_E       reference objective value. Not necessary.

 All these parameters, except S0 and T are contained in an instance
 of struct ae_sa_parameters.
*/
struct ae_mapping *ae_sa(struct sa_level *salevels, size_t maxlevels,
			 struct ae_mapping *S0, double T,
			 struct ae_sa_parameters *params)
{
	int k = 0;
	int rejects = 0;
	double E, E_best, E_new;
	struct ae_mapping *S, *S_best, *S_new;
	size_t level = 0;
	int levelrecorded = 0;
	int npes = S0->arch->npes;
	struct optstate *os;
	double diff;

	os = create_optstate(20);

	assert(T > 0.0);
	assert(params->Tf > 0.0);

	E = params->objective(S0);
	E_best = E;
	S = ae_fork_mapping(S0);
	S_best = ae_fork_mapping(S0);
	S_new = ae_fork_mapping(S0);

	while (1) {
		if (params->leveloptimization) {
			assert(salevels != NULL);
			if (!levelrecorded) {
				T = salevels[level++].T;
				levelrecorded = 1;
			}
		} else {
			if (!levelrecorded && salevels != NULL) {
				assert(level < maxlevels);
				salevels[level].T = T;
				salevels[level++].objective = E_best;
				levelrecorded = 1;
			}
		}

		/*
		 * HACK: change the number of PEs, and restore the value after
		 * move()
		 */
		if (params->maxpes)
			S0->arch->npes = params->maxpes;

		params->move(S_new, S, T, os);

		/* Restore the number of PEs */
		if (params->maxpes)
			S0->arch->npes = npes;

		E_new = params->objective(S_new);

		append_optstate_move(os, E, E_new);

		diff = cost_diff(E, E_new);
		if (diff < 0 || ae_randd(0, 1.0) < params->acceptor(diff, T, params)) {
			ae_copy_mapping(S, S_new);
			E = E_new;

			if (cost_diff(E_best, E_new) < 0) {
				ae_copy_mapping(S_best, S_new);
				E_best = E_new;
				printf("best_sa_cost_so_far: %e %lld %.9f %.3f %.2f %.9f\n", T, S->result->evals, E_best, params->ref_E / E_best, S->schedule->arbavginqueue, S->schedule->arbavgtime);
			}
			rejects = 0;

		} else if (T <= params->Tf) {
			if (rejects >= params->max_rejects)
				break;
			rejects++;
		}

		k++;

		if (k % params->schedule_max == 0) {

			if (params->leveloptimization && level == maxlevels)
				break;

			printf("best_sa_cost_so_far: %e %lld %.9f %.3f\n", T, S->result->evals, E_best, params->ref_E / E_best);
			fflush(stdout);

			T = params->schedule(T, params);
			
			printf("Transition_prob: T %.6f 0.001 %.6f 0.010 %.6f 0.100 %.6f\n",
			       T,
			       params->acceptor(0.001 * params->ref_E, T, params),
			       params->acceptor(0.010 * params->ref_E, T, params),
			       params->acceptor(0.100 * params->ref_E, T, params));

			if (params->greedy) {
				ae_copy_mapping(S, S_best);
				E = E_best;
			}

			levelrecorded = 0;
		}
	}

	ae_free_mapping(S);
	ae_free_mapping(S_new);

	free_optstate(os);

	return S_best;
}

void ae_sa_autotemp(struct ae_sa_parameters *params, struct ae_mapping *map)
{
	int i;
	double perf;
	double maxperf;
	double minperf;
	struct ae_pe *pe;
	double k = 2.0;

	minperf = 1E10;
	maxperf = 0.0;

	for (i = 0; i < map->arch->npes; i++) {
		pe = map->arch->pes[i];

		/* Compute operations/s value */
		perf = 1.0 / ae_pe_computation_time(pe, 1);

		if (perf < minperf)
			minperf = perf;

		if (perf > maxperf)
			maxperf = perf;
	}

	if (map->tasks)
		ae_stg_sa_autotemp(params, minperf, maxperf, k, map);
	else
		ae_kpn_autotemp(params, minperf, maxperf, k, map);

	assert(params->T0 > 0.0);
	assert(params->Tf > 0.0);
	assert(params->T0 >= params->Tf);
}

#define DIVISOR_LOWER_LIMIT 1E-14
 /* one in a million probability */
#define EXPONENT_UPPER_LIMIT 14

static double inverse_exponential_acceptor(double dE, double T,
					   struct ae_sa_parameters *params)
{
	double divisor, exponent;
	divisor = params->acceptor_param1 * T;
	if (divisor < DIVISOR_LOWER_LIMIT) {
		fprintf(stderr, "sa acceptor divisor too small\n");
		return 0.0;
	}
	exponent = dE / divisor;
	if (exponent > EXPONENT_UPPER_LIMIT)
		return 0.0;
	return 2.0 * params->zero_transition_prob / (1 + exp(exponent));
}

static double exponential_acceptor(double dE, double T,
				   struct ae_sa_parameters *params)
{
	double exponent;
	double divisor;
	divisor = params->acceptor_param1 * T;
	if (divisor < DIVISOR_LOWER_LIMIT) {
		fprintf(stderr, "sa acceptor divisor too small\n");
		return 0.0;
	}
	exponent = -dE / divisor;
	if (exponent >= 0)
		return 1.0;
	return exp(exponent);
}

static double special_1_acceptor(double dE, double T, struct ae_sa_parameters *params)
{
	double C0 = 2 * params->acceptor_param1;
	double divisor = 2 * C0 * T;
	if (divisor < DIVISOR_LOWER_LIMIT) {
		fprintf(stderr, "sa acceptor divisor too small\n");
		return 0.0;
	}
	return MAX(0, 1 - dE / divisor);
}

static double geometric_schedule(double T, struct ae_sa_parameters *params)
{
	return T * params->schedule_param1;
}


/* read simulated annealing parameters from file f. */
struct ae_sa_parameters *ae_sa_read_parameters(FILE *f)
{
	struct ae_sa_parameters *p;

	/* Name "exponential" is misleading, it's "inverse exponential", really */
	char *acceptors[] = {"exponential", "original", "special_1", NULL};

	char *schedules[] = {"geometric", NULL};
	char **heuristics_names;
	struct mh_heuristics *h;
	int ret;
	int i, nheuristics;
	char *s;
	/* obligatory mask */
	int nobmask = 0;

	if ((p = calloc(1, sizeof(p[0]))) == NULL)
		ae_err("no memory for sa context\n");

	p->zero_transition_prob = 0.5;

	nheuristics = 0;
	h = mh_heuristics;
	while (h->name != NULL) {
		h++;
		nheuristics++;
	}
	assert(nheuristics > 0);
	h = mh_heuristics;
	MALLOC_ARRAY(heuristics_names, nheuristics + 1);
	for (i = 0; i < nheuristics; i++) {
		if ((heuristics_names[i] = strdup(h->name)) == NULL)
			ae_err("not enough memory for heuristics names\n");
		h++;
	}
	heuristics_names[i] = NULL;

	while (1) {
		s = ae_get_word(f);
		if (!strcmp(s, "end_simulated_annealing")) {
			free(s);
			s = NULL;
			break;
		}

		if (!strcmp("max_rejects", s)) {
			nobmask |= 1;
			p->max_rejects = ae_get_int(f);
		} else if (!strcmp("schedule_max", s)) {
			nobmask |= 2;
			p->schedule_max = ae_get_int(f);
		} else if (!strcmp("T0", s)) {
			nobmask |= 4;
			p->T0 = ae_get_double(f);
		} else if (!strcmp("Tf", s)) {
			nobmask |= 8;
			p->Tf = ae_get_double(f);
		} else if (!strcmp("acceptor", s)) {
			nobmask |= 16;
			ret = ae_match_alternatives(acceptors, f);
			if (ret == 0)
				p->acceptor = inverse_exponential_acceptor;
			else if (ret == 1)
				p->acceptor = exponential_acceptor;
			else if (ret == 2)
				p->acceptor = special_1_acceptor;
			else
				ae_err("unknown sa acceptor\n");
			printf("sa_acceptor: %s\n", acceptors[ret]);
		} else if (!strcmp("schedule", s)) {
			nobmask |= 32;
			ret = ae_match_alternatives(schedules, f);
			if (ret != 0)
				ae_err("unknown sa schedule\n");
			p->schedule = geometric_schedule;
			p->schedule_param1 = ae_get_double(f);
		} else if (!strcmp("heuristics", s)) {
			nobmask |= 64;
			ret = ae_match_alternatives(heuristics_names, f);
			if (ret < 0)
				ae_err("unknown sa heuristics\n");
			ae_sa_set_heuristics(p, heuristics_names[ret]);
		} else if (!strcmp("zero_transition_prob", s)) {
			double ztp = ae_get_double(f);
			assert(ztp >= 0.0 && ztp <= 1.0);
			p->zero_transition_prob = ztp;
		} else {
			ae_err("Unknown sa parameter: %s\n", s);
		}

		free(s);
		s = NULL;
	}

	assert(nobmask == 127);

	free(heuristics_names);

	return p;
}


void ae_sa_set_heuristics(struct ae_sa_parameters *p, char *name)
{
	struct mh_heuristics *h = mh_heuristics;
	while (h->name != NULL) {
		if (strcmp(h->name, name) == 0) {
			if ((p->heuristics_name = strdup(name)) == NULL)
				ae_err("no memory for heuristics name\n");
			p->move = h->f;
			break;
		}
		h++;
	}
	if (h->name == NULL)
		ae_err("unknown heuristics for sa: %s\n", name);
}
