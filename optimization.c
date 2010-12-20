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

#include "optimization.h"

#include "arexbasic.h"
#include "gm.h"
#include "input.h"
#include "mappingheuristics.h"
#include "optimalsubset.h"
#include "result.h"
#include "sa.h"
#include "schedule.h"
#include "randommapping.h"
#include "fast_premapping.h"
#include "genetic_algorithm.h"
#include "arch.h"
#include "bruteforce.h"
#include "neighborhood-test-mapping.h"
#include "support.h"


enum optmethod {
	OPT_INVALID,
	OPT_OPTIMAL_SUBSET_MAPPING,
	OPT_RANDOM_MAPPING,
	OPT_GROUP_MIGRATION,
	OPT_SIMULATED_ANNEALING,
	OPT_SIMULATED_ANNEALING_AUTOTEMP,
	OPT_FAST_HYBRID_GM_SA,
	OPT_FAST_HYBRID_GM_SA_AUTOTEMP,
	OPT_SLOW_HYBRID_GM_SA,
	OPT_SLOW_HYBRID_GM_SA_AUTOTEMP,
	OPT_ITERATED_SIMULATED_ANNEALING,
	OPT_ITERATED_SIMULATED_ANNEALING_AUTOTEMP,
	OPT_GROUP_MIGRATION_2,
	OPT_GROUP_MIGRATION_RANDOM,
	OPT_GENETIC_ALGORITHM,
	OPT_SIMULATED_ANNEALING_AUTOTEMP_2,
	OPT_SIMULATED_ANNEALING_AUTOTEMP_3,
	OPT_BRUTE_FORCE,
	OPT_OSM_SA,
	OPT_SIMULATED_ANNEALING_LEVELS,
	OPT_NEIGHBORHOOD_TEST,
	OPT_BRUTE_FORCE_WITH_SCHEDULE,
	OPT_BRUTE_FORCE_MAP_SCHEDULE,
};


struct optmethods {
	const char *name;
	enum optmethod e;
};

static const struct optmethods optmethods[] = {
	{"optimal_subset_mapping", OPT_OPTIMAL_SUBSET_MAPPING},
	{"random_mapping", OPT_RANDOM_MAPPING},
	{"group_migration", OPT_GROUP_MIGRATION},
	{"simulated_annealing", OPT_SIMULATED_ANNEALING},
	{"simulated_annealing_autotemp", OPT_SIMULATED_ANNEALING_AUTOTEMP},
	{"fast_hybrid_gm_sa", OPT_FAST_HYBRID_GM_SA},
	{"fast_hybrid_gm_sa_autotemp", OPT_FAST_HYBRID_GM_SA_AUTOTEMP},
	{"slow_hybrid_gm_sa", OPT_SLOW_HYBRID_GM_SA},
	{"slow_hybrid_gm_sa_autotemp", OPT_SLOW_HYBRID_GM_SA_AUTOTEMP},
	{"iterated_simulated_annealing", OPT_ITERATED_SIMULATED_ANNEALING},
	{"iterated_simulated_annealing_autotemp", OPT_ITERATED_SIMULATED_ANNEALING_AUTOTEMP},
	{"group_migration_2", OPT_GROUP_MIGRATION_2},
	{"group_migration_random", OPT_GROUP_MIGRATION_RANDOM},
	{"genetic_algorithm", OPT_GENETIC_ALGORITHM},
	{"simulated_annealing_autotemp2", OPT_SIMULATED_ANNEALING_AUTOTEMP_2},
	{"simulated_annealing_autotemp3", OPT_SIMULATED_ANNEALING_AUTOTEMP_3},
	{"brute_force", OPT_BRUTE_FORCE},
	{"osm_sa", OPT_OSM_SA},
	{"simulated_annealing_levels", OPT_SIMULATED_ANNEALING_LEVELS},
	{"neighborhood_test", OPT_NEIGHBORHOOD_TEST},
	{"brute_force_with_schedule", OPT_BRUTE_FORCE_WITH_SCHEDULE},
	{"brute_force_map_schedule", OPT_BRUTE_FORCE_MAP_SCHEDULE},
	{NULL, 0},
};


struct osm_sa_parameters {
  struct ae_osm_parameters *osm;
  struct ae_sa_parameters *sa;
};


static struct ae_mapping *gm(struct ae_mapping *map, double initial);
static struct ae_mapping *fast_hybrid_gm_sa(struct ae_mapping *map, double initial);
static struct ae_mapping *slow_hybrid_gm_sa(struct ae_mapping *map, double initial);
static struct ae_mapping *iterated_sa(struct ae_mapping *map, double initial);
static struct ae_mapping *osm(struct ae_mapping *map, double initial);
static struct ae_mapping *sa(struct ae_mapping *map, double initial);

void append_optstate_move(struct optstate *os, double old, double new)
{
	os->ringpos = (os->ringpos + 1) % os->ringsize;
	os->moves[os->ringpos] = (struct optmove) {.acceptedobjective = old,
						   .newobjective = new};
	if (os->ringused < os->ringsize)
		os->ringused++;
}

double cost_diff(double old, double new)
{
	double diff = new - old;
	if (ae_config.find_maximum)
		diff = -diff;
	return diff;
}

struct optstate *create_optstate(size_t ringsize)
{
	struct optstate *os;
	assert(ringsize > 0);
	CALLOC_ARRAY(os, 1);
	os->ringsize = ringsize;
	MALLOC_ARRAY(os->moves, os->ringsize);
	return os;
}

void free_optstate(struct optstate *os)
{
	free(os->moves);
	os->moves = NULL;
	os->ringsize = os->ringused = os->ringpos = -1;
	free(os);
}

int opt_move_probabilities(struct optmoveprobabilities *ps,
			    struct optstate *os)
{
	size_t n;
	size_t pos = os->ringpos;
	size_t nworsening = 0;
	size_t nsame = 0;
	size_t nbetter = 0;
	double acco;
	double newo;

	memset(ps, 0, sizeof *ps);

	if (os->ringused < 10)
		return 0;

	for (n = os->ringused; n > 0; n--) {
		acco = os->moves[pos].acceptedobjective;
		newo = os->moves[pos].newobjective;

		if (newo < acco)
			nbetter++;
		else if (newo == acco)
			nsame++;
		else
			nworsening++;

		assert(pos < os->ringsize);
		if (pos == 0)
			pos = os->ringsize - 1;
		else
			pos--;
	}

	ps->pworse = ((double) nworsening) / os->ringused;
	ps->psame = ((double) nsame) / os->ringused;
	ps->pbetter = ((double) nbetter) / os->ringused;

	return 1;
}

static void log_objective(double obj, struct ae_mapping *map)
{
	struct ae_result *r = map->result;

	if (r != NULL) {
		if (sa_output_file != NULL) {
			if (r->evals >= r->allocated) {
				if (r->allocated == 0)
					r->allocated++;
				else
					r->allocated *= 2;

				REALLOC_ARRAY(r->time, r->allocated);
				REALLOC_ARRAY(r->objective, r->allocated);
			}

			r->time[r->evals] = map->schedule->schedule_length;
			r->objective[r->evals] = obj;
		}

		r->evals++;
	}
}


static double default_objective(struct ae_mapping *map)
{
	double obj;
	map->appmodel->schedule(map);
	obj = map->schedule->schedule_length;
	log_objective(obj, map);
	return obj;
}


static double time_power_objective(struct ae_mapping *map)
{
	double statE, dynE;
	double obj;
	map->appmodel->schedule(map);
	ae_energy(NULL, &statE, &dynE, map);
	assert(statE >= 0.0);
	assert(dynE >= 0.0);
	obj = statE + dynE;
	log_objective(obj, map);
	return obj;
}


struct ae_optimization *ae_create_optimization_context(void)
{
	struct ae_optimization *opt;
	CALLOC_ARRAY(opt, 1);
	return opt;
}

static struct ae_mapping *brute_force(struct ae_mapping *map, double initial)
{
	return ae_brute_force(map, initial, OPT_MAPPING);
}

static struct ae_mapping *brute_force_with_schedule(struct ae_mapping *map, double initial)
{
	return ae_brute_force(map, initial, OPT_MAPPING | OPT_SCHEDULING);
}

static struct ae_mapping *brute_force_map_schedule(struct ae_mapping *map, double initial)
{
	struct ae_mapping *newmap;
	struct ae_mapping *bestmap;
	newmap = ae_brute_force(map, initial, OPT_MAPPING);
	bestmap = ae_brute_force(newmap, initial, OPT_SCHEDULING);
	ae_free_mapping(newmap);
	return bestmap;
}

static struct ae_mapping *genetic_algorithm(struct ae_mapping *map,
					    double initial)
{
	struct ga_parameters *p = map->optimization->params;

	p->objective = map->optimization->objective;
	p->initial_cost = initial;

	return ae_genetic_algorithm(map, p);
}


static struct ae_mapping *gm(struct ae_mapping *map, double initial)
{
	initial = initial;
	return ae_gm(map, 0);
}


static struct ae_mapping *gm_random(struct ae_mapping *map, double initial)
{
	initial = initial;
	return ae_gm(map, 1);
}


static struct ae_mapping *gm2(struct ae_mapping *map, double initial)
{
	initial = initial;
	return ae_gm2(map);
}


static struct ae_mapping *help_iterated_sa(struct ae_mapping *map,
					   double initial, int use_gm)
{
	struct ae_mapping *map2, *map3;
	struct ae_sa_parameters *saparams;
	double T = 1.0;

	saparams = map->optimization->params;

	map2 = ae_fork_mapping(map);

	while (T >= saparams->Tf) {

		saparams->T0 = T;
		map3 = sa(map2, initial);
		ae_copy_mapping(map2, map3);
		ae_free_mapping(map3);

		if (use_gm) {
			map3 = ae_gm(map2, 0);
			ae_copy_mapping(map2, map3);
			ae_free_mapping(map3);
		}

		T /= 2.0;
	}

	return map2;
}


static struct ae_mapping *fast_hybrid_gm_sa(struct ae_mapping *map,
					    double initial)
{
	struct ae_mapping *map2, *map3;
	map2 = sa(map, initial);
	map3 = ae_gm(map2, 0);
	ae_free_mapping(map2);
	return map3;
}


static struct ae_mapping *slow_hybrid_gm_sa(struct ae_mapping *map,
					    double initial)
{
	return help_iterated_sa(map, initial, 1);
}


static struct ae_mapping *iterated_sa(struct ae_mapping *map,
				      double initial)
{
	return help_iterated_sa(map, initial, 0);
}


static struct ae_mapping *osm(struct ae_mapping *map, double initial)
{
	struct ae_osm_parameters *osmparams;
	struct ae_optimization *opt = map->optimization;
	initial = initial;
	osmparams = opt->params;
	ae_osm_init(osmparams, map->ntasks, map->arch->npes);
	osmparams->objective = opt->objective;
	return ae_osm(map, osmparams);
}

static struct osm_sa_parameters *read_osm_sa_parameters(FILE *f)
{
	struct osm_sa_parameters *params;
	MALLOC_ARRAY(params, 1);

	params->osm = ae_osm_read_parameters(f);

	params->sa = ae_sa_read_parameters(f);
	params->sa->autotemp = 1;

	return params;
}

static struct ae_mapping *osm_sa(struct ae_mapping *map, double initial)
{
	struct ae_mapping *osmres, *sares;
	struct osm_sa_parameters *params = map->optimization->params;

	map->optimization->params = params->osm;
	osmres = osm(map, initial);

	map->optimization->params = params->sa;
	sares = sa(osmres, initial);

	ae_free_mapping(osmres);

	return sares;
}

static struct ae_mapping *random_mapping(struct ae_mapping *map,
					 double initial)
{
	return ae_random_mapping(map, initial);
}


/* call simulated annealing algorithm.
   two parameters of sa are handled in a special way. if max_rejects == -1,
   it is set to Tasks*(PEs - 1). if schedule_max == -1 it is set to
   Tasks*(PEs - 1) */
static struct ae_mapping *sa(struct ae_mapping *map, double initial)
{
	struct ae_sa_parameters *saparams;
	struct ae_optimization *opt = map->optimization;
	double T;
	struct ae_mapping *newmap;

	saparams = opt->params;
	saparams->ref_E = initial;
	saparams->objective = opt->objective;
	saparams->acceptor_param1 = initial / 2.0;

	if (saparams->autotemp)
		ae_sa_autotemp(saparams, map);

	T = saparams->T0;

	if (saparams->max_rejects == -1)
		saparams->max_rejects = map->ntasks * (map->arch->npes - 1);

	if (saparams->schedule_max == -1)
		saparams->schedule_max = map->ntasks * (map->arch->npes - 1);

	newmap = ae_sa(NULL, 0, map, T, saparams);

	switch (saparams->autotemp) {
	case 0:
	case 1:
		return newmap;

	case 2:
		/* Simulated annealing autotemp version 2 (50% more iterations) */
		T = sqrt(saparams->T0 * saparams->Tf);
		break;

	case 3:
		/* Simulated annealing autotemp version 3 (100% more iterations) */
		T = saparams->T0;
		break;

	default:
		ae_err("Unknown SA autotemp version: %d\n", saparams->autotemp);
	}

	map = ae_sa(NULL, 0, newmap, T, saparams);

	ae_free_mapping(newmap);

	return map;
}

/* Highest objective first */
static int compare_level_objective(const void *a, const void *b)
{
	const struct sa_level *x = a;
	const struct sa_level *y = b;
	if (x->objective < y->objective)
		return 1;
	else if (y->objective < x->objective)
		return -1;
	return 0;
}

/* Highest temperature first */
static int compare_level_T(const void *a, const void *b)
{
	const struct sa_level *x = a;
	const struct sa_level *y = b;
	if (x->T < y->T)
		return 1;
	else if (y->T < x->T)
		return -1;
	return 0;
}

static struct ae_mapping *sa_with_levels(struct ae_mapping *map, double initial)
{
	struct ae_sa_parameters *saparams;
	struct ae_optimization *opt = map->optimization;
	double T;
	struct ae_mapping *map2;
	struct ae_mapping *map3;

	struct sa_level *salevels;
	size_t maxlevels = 10000;
	size_t nlevels;
	size_t l;
	double x;
	double y;
	size_t optlevels;

	saparams = opt->params;
	saparams->ref_E = initial;
	saparams->objective = opt->objective;
	saparams->acceptor_param1 = initial / 2.0;

	ae_sa_autotemp(saparams, map);

	T = saparams->T0;

	CALLOC_ARRAY(salevels, maxlevels);

	saparams->maxpes = 2;
	saparams->leveloptimization = 0;
	saparams->max_rejects = map->ntasks * (saparams->maxpes - 1);
	saparams->schedule_max = map->ntasks * (saparams->maxpes - 1);

	map2 = ae_sa(salevels, maxlevels, map, T, saparams);

	for (l = 0; l < (maxlevels - 1); l++) {
		x = salevels[l].objective;
		y = salevels[l + 1].objective;
		if (y == 0) {
			salevels[l].objective = 0;
			break;
		} else {
			salevels[l].objective = x - y;
		}
	}
	nlevels = l + 1;

	/* sort to greatest improvements first */
	qsort(salevels, nlevels, sizeof salevels[0], compare_level_objective);

	optlevels = MAX((nlevels * 50) / 100, 1);

	printf("Switching to level mode: %zu -> %zu\n", nlevels, optlevels);

	printf("Fix moves/templevel value with respect to level improvements\n");

	qsort(salevels, optlevels, sizeof salevels[0], compare_level_T);

	saparams->maxpes = 0;
	saparams->leveloptimization = 1;
	saparams->max_rejects = 2 * map->ntasks * (map->arch->npes - 1);
	saparams->schedule_max = 2 * map->ntasks * (map->arch->npes - 1);

	/* Interesting: starting from map2 leads to worse results */

	map3 = ae_sa(salevels, optlevels, map, T, saparams);
	ae_free_mapping(map2);
	return map3;
}

struct ae_mapping *ae_optimize(struct ae_mapping *map)
{
	struct ae_mapping *newmap;
	struct ae_optimization *opt = map->optimization;
	double best, initial;

	ae_init_result(map);

	ae_init_schedule(map);

	initial = opt->objective(map);

	if (map->result != NULL) {
		map->result->initial = initial;
		map->result->initial_time = map->schedule->schedule_length;
		map->result->initial_memory = 1;
	}

	if (ae_config.fast_premapping)
		map = ae_fast_premapping(map);

	assert(opt->method != NULL);

	newmap = opt->method(map, initial);

	best = opt->objective(newmap);
	if (map->result != NULL) {
		map->result->best = best;
		map->result->best_time = map->schedule->schedule_length;
		map->result->best_memory = 1;
	}

	if (map->result != NULL) {
		ae_print_result(newmap, map);
		ae_free_result(map);
	} else {
		fprintf(stderr, "best objective: %.9lf\n", best);
		fprintf(stderr, "gain: %.3lf\n", initial / best);
		ae_print_mapping(newmap);
	}
	return newmap;
}


void ae_read_optimization_parameters(struct ae_optimization *opt, FILE *f)
{
	char *objectives[] = {"execution_time",       /* 0 */
			      "execution_time_power", /* 1 */
			      NULL};
	int ret;
	char *methodname;
	int i;
	enum optmethod optmethod;

	ae_match_word("objective_function", f);
	ret = ae_match_alternatives(objectives, f);
	switch (ret) {
	case 0:
		/* time optimization */
		opt->objective = default_objective;
		break;
	case 1:
		/*
		 * time-power optimization with k parameter. k = 0 implies pure
		 * time optimization case.
		 */
		ae_match_word("k", f);
		opt->power_k = ae_get_double(f);
		opt->objective = time_power_objective;
		break;
	default:
		ae_err("unknown objective\n");
	}

	opt->objective_name = xstrdup(objectives[ret]);

	ae_match_word("method", f);
	methodname = ae_get_word(f);
	optmethod = OPT_INVALID;
	for (i = 0; optmethods[i].name != NULL; i++) {
		if (strcmp(methodname, optmethods[i].name) == 0) {
			optmethod = optmethods[i].e;
			break;
		}
	}
	opt->method_name = methodname;
	switch (optmethod) {
	case OPT_OPTIMAL_SUBSET_MAPPING:
		opt->method = osm;
		opt->params = ae_osm_read_parameters(f);
		break;
	case OPT_RANDOM_MAPPING:
		opt->method = random_mapping;
		opt->params = ae_random_read_parameters(f);
		break;
	case OPT_GROUP_MIGRATION:
		opt->method = gm;
		break;
	case OPT_SIMULATED_ANNEALING:
		opt->method = sa;
		opt->params = ae_sa_read_parameters(f);
		break;
	case OPT_SIMULATED_ANNEALING_AUTOTEMP:
		opt->method = sa;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 1;
		break;
	case OPT_FAST_HYBRID_GM_SA:
		opt->method = fast_hybrid_gm_sa;
		opt->params = ae_sa_read_parameters(f);
		break;
	case OPT_FAST_HYBRID_GM_SA_AUTOTEMP:
		opt->method = fast_hybrid_gm_sa;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 1;
		break;
	case OPT_SLOW_HYBRID_GM_SA:
		opt->method = slow_hybrid_gm_sa;
		opt->params = ae_sa_read_parameters(f);
		break;
	case OPT_SLOW_HYBRID_GM_SA_AUTOTEMP:
		opt->method = slow_hybrid_gm_sa;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 1;
		break;
	case OPT_ITERATED_SIMULATED_ANNEALING:
		opt->method = iterated_sa;
		opt->params = ae_sa_read_parameters(f);
		break;
	case OPT_ITERATED_SIMULATED_ANNEALING_AUTOTEMP:
		opt->method = iterated_sa;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 1;
		break;
	case OPT_GROUP_MIGRATION_2:
		opt->method = gm2;
		break;
	case OPT_GROUP_MIGRATION_RANDOM:
		opt->method = gm_random;
		break;
	case OPT_GENETIC_ALGORITHM:
		opt->method = genetic_algorithm;
		opt->params = ae_ga_read_parameters(f);
		break;
	case OPT_SIMULATED_ANNEALING_AUTOTEMP_2:
		opt->method = sa;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 2;
		break;
	case OPT_SIMULATED_ANNEALING_AUTOTEMP_3:
		opt->method = sa;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 3;
		break;
	case OPT_BRUTE_FORCE:
		opt->method = brute_force;
		break;
	case OPT_OSM_SA:
		opt->method = osm_sa;
		opt->params = read_osm_sa_parameters(f);
		break;
	case OPT_SIMULATED_ANNEALING_LEVELS:
		opt->method = sa_with_levels;
		opt->params = ae_sa_read_parameters(f);
		((struct ae_sa_parameters *) opt->params)->autotemp = 1;
		break;
	case OPT_NEIGHBORHOOD_TEST:
		opt->method = ae_neighborhood_test_mapping;
		opt->params = ae_ntm_read_parameters(f);
		break;
	case OPT_BRUTE_FORCE_WITH_SCHEDULE:
		opt->method = brute_force_with_schedule;
		break;
	case OPT_BRUTE_FORCE_MAP_SCHEDULE:
		opt->method = brute_force_map_schedule;
		break;
	default:
		ae_err("Invalid optimization method: %s\n", methodname);
	}
}
