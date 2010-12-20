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

#include "neighborhood-test-mapping.h"
#include "mapping.h"
#include "arexbasic.h"
#include "optimization.h"
#include "result.h"
#include "mappingheuristics.h"
#include "input.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

struct lscounter {
	size_t nworse;
	size_t nsame;
	size_t nbetter;
};

static int local_search(struct ae_mapping *startmap, double startcost)
{
	struct ae_mapping *map;
	double cost;
	struct optstate *os;
	struct optmoveprobabilities ps;
	int i;
	int oldpeid;
	int peid;
	int n;
	int c1, c2, c3, c4;

	map = ae_fork_mapping(startmap);

	os = create_optstate(map->ntasks * (map->arch->npes - 1));

	for (i = 0; i < map->ntasks; i++) {
		if (map->isstatic[i])
			continue;
		oldpeid = map->mappings[i];
		for (peid = 0; peid < map->arch->npes; peid++) {
			if (peid == oldpeid)
				continue;
			ae_set_mapping(map, i, peid);
			cost = map->optimization->objective(map);
			append_optstate_move(os, startcost, cost);
		}
		ae_set_mapping(map, i, oldpeid);
	}

	assert(opt_move_probabilities(&ps, os));

	n = 1;
	c1 = (ps.psame == 0 && ps.pbetter < 0.5);
	c2 = (ps.pworse >= 0.75);
	c3 = (ps.psame >= 0.25);
	c4 = (ps.pworse <= 0.25);
	if (!c1 && !c2 && (c3 || c4))
		n = 2;

	/* printf("move probabilities: %.3f %.3f %.3f %d %d %d %d : n = %d\n", ps.pworse, ps.psame, ps.pbetter, c1, c2, c3, c4, n); */

	ae_free_mapping(map);
	free_optstate(os);

	return n;
}

static void counter_print(const char *title, struct lscounter *counter)
{
	size_t n = counter->nworse + counter->nsame + counter->nbetter;
	double pworse = 0.0;
	double psame = 0.0;
	double pbetter = 0.0;

	if (n > 0) {
		pworse = ((double) counter->nworse) / n;
		psame = ((double) counter->nsame) / n;
		pbetter = ((double) counter->nbetter) / n;
	}

	printf("total probabilities %s: n %zd worse %.3f same %.3f better %.3f\n", title, n, pworse, psame, pbetter);
}

static void counter_add(struct lscounter *sum, struct lscounter *a, struct lscounter *b)
{
	*sum = *a;
	sum->nworse += b->nworse;
	sum->nsame += b->nsame;
	sum->nbetter += b->nbetter;
}

struct ae_mapping *ae_neighborhood_test_mapping(struct ae_mapping *map, double initial)
{
	struct ae_mapping *newmap;
	struct ae_mapping *bestmap;
	double oldcost;
	double newcost;
	double bestcost;
	size_t iteration;
	size_t maxiteration;
	int print_progress = 0;
	int tochange;
	int changemax;
	int success;
	struct lscounter counterall = {};
	struct lscounter counter1 = {};
	struct lscounter counter2 = {};
	struct lscounter *counter;
	struct ae_ntm_parameters *p = map->optimization->params;

	assert(!ae_config.find_maximum);

	changemax = ae_config_get_int(&success, "changemax");
	assert(success == 0 || changemax == 1 || changemax == 2);
	if (!success) {
		if (p->changemax != 0)
			changemax = p->changemax;
		else
			changemax = 2;
	}
	printf("neighborhood_test_mapping: changemax: %d\n", changemax);

	maxiteration = ae_config_get_size_t(&success, "itermax");
	assert(success == 0 || maxiteration > 0);
	if (!success) {
		if (p->itermax != 0)
			maxiteration = p->itermax;
		else
			maxiteration = 1000;
	}
	printf("neighborhood_test_mapping: itermax: %zd\n", maxiteration);

	newmap = ae_fork_mapping(map);
	ae_randomize_mapping(newmap);
	newcost = map->optimization->objective(newmap);

	bestmap = ae_fork_mapping(newcost < initial ? newmap : map);
	bestcost = MIN(newcost, initial);

	for (iteration = 0; iteration < maxiteration; iteration++) {
		oldcost = newcost;

		tochange = local_search(newmap, newcost);
		if (tochange > changemax)
			tochange = changemax;
		counter = (tochange == 2) ? &counter2 : &counter1;

		ae_randomize_n_task_mappings(newmap, tochange);

		newcost = map->optimization->objective(newmap);
		/*printf("accepted_objective: %.9lf\n", newcost);*/

		if (newcost < oldcost)
			counter->nbetter++;
		else if (newcost == oldcost)
			counter->nsame++;
		else
			counter->nworse++;

		if (newcost < bestcost) {
			bestcost = newcost;
			ae_copy_mapping(bestmap, newmap);
			print_progress = 1;
		} else {
			print_progress = (map->result->evals % 100) == 0 ? 1 : 0;
		}

		if (print_progress)
			printf("best_neighborhood_test_cost_so_far: %lld %.9f %.3f %.9lf\n", map->result->evals, bestcost, initial / bestcost, newcost);
	}

	counter_add(&counterall, &counter1, &counter2);

	counter_print("1", &counter1);
	counter_print("2", &counter2);
	counter_print("all", &counterall);

	ae_free_mapping(newmap);

	return bestmap;
}

struct ae_ntm_parameters *ae_ntm_read_parameters(FILE *f)
{
	struct ae_ntm_parameters *p;
	char *ps;
	long long ll;

	if ((p = calloc(1, sizeof(p[0]))) == NULL)
		ae_err("%s: no memory\n", __func__);

	while (1) {
		ps = ae_get_word(f);
		if (strcmp(ps, "end_optimization") == 0)
			break;
		if (strcmp(ps, "changemax") == 0) {
			p->changemax = ae_get_uint(f);
			assert(p->changemax >= 0);
		} else if (strcmp(ps, "itermax") == 0) {
			ll = ae_get_long_long(f);
			assert(ll >= 0);
			p->itermax = (size_t) ll;
		} else {
			ae_err("Invalid ntm parameter: %s\n", ps);
		}
		free(ps);
		ps = NULL;
	}

	return p;
}
