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

#include <string.h>
#include <assert.h>

#include "genetic_algorithm.h"

#include "mapping.h"
#include "arexbasic.h"
#include "input.h"
#include "result.h"

static struct individual *fork_individual(struct individual *individual,
					  struct ga_parameters *p);
static void free_individual(struct individual *individual);
static struct individual *map_to_individual(struct ae_mapping *map,
					    struct ga_parameters *p);
static void point_mutation(struct ae_mapping *map, unsigned int taskid);


static double fitness(struct individual *individual, struct ga_parameters *p)
{
    return 1.0 / p->objective(individual->map);
}


static inline double fitness_to_cost(double fitness)
{
    return 1.0 / fitness;
}


/* We'll create an initial population, and mutate all but one individual.
   It should save a few generations of crossovers and mutation from
   optimisation time */
static struct individual **create_population(struct ae_mapping *map,
					     struct ga_parameters *p)
{
    struct individual *starter;
    struct individual **population;
    size_t i;
    unsigned int taskid;

    starter = map_to_individual(map, p);

    MALLOC_ARRAY(population, p->population_size);

    population[0] = fork_individual(starter, p);

    taskid = 0;

    for (i = 1; i < p->population_size; i++) {
	population[i] = fork_individual(starter, p);

	point_mutation(population[i]->map, taskid);

	population[i]->fitness = fitness(population[i], p);

	taskid = (taskid + 1) % map->ntasks;
    }

    free_individual(starter);

    return population;
}

static void single_point_co(struct individual *child,
			    struct individual *parent1,
			    struct individual *parent2)
{
    struct ae_mapping *map = parent1->map;
    size_t cutpoint;
    size_t i;

    cutpoint = ae_randi(0, map->ntasks + 1);

    /* Copy mappings [0, cutpoint) from parent1, and [cutpoint, ntasks)
       from parent2. Note, parent1 and parent2 may have been swapped. */
    for (i = 0; i < cutpoint; i++)
	child->map->mappings[i] = parent1->map->mappings[i];

    for (; i < map->ntasks; i++)
	child->map->mappings[i] = parent2->map->mappings[i];
}

static void two_point_co(struct individual *child,
			 struct individual *parent1,
			 struct individual *parent2)
{
    struct ae_mapping *map = parent1->map;
    size_t a;
    size_t b;
    size_t i;

    /*
     * Get random points for two point crossover: 0 <= a <= b <= ntasks.
     * Copy mappings [0, a) from parent1, [a, b) from parent2, and then
     * [b, ntasks) from parent1.
     */
    a = ae_randi(0, map->ntasks + 1);
    b = ae_randi(0, map->ntasks + 1);
    if (b < a) {
	i = a;
	a = b;
	b = i;
    }

    for (i = 0; i < a; i++)
	child->map->mappings[i] = parent1->map->mappings[i];
    for (; i < b; i++)
	child->map->mappings[i] = parent2->map->mappings[i];
    for (; i < map->ntasks; i++)
	child->map->mappings[i] = parent1->map->mappings[i];
}

static void uniform_co(struct individual *child,
		       struct individual *parent1,
		       struct individual *parent2)
{
    struct ae_mapping *map = parent1->map;
    size_t i;

    for (i = 0; i < map->ntasks; i++) {
	if (ae_randi(0, 2) == 0)
	    child->map->mappings[i] = parent1->map->mappings[i];
	else
	    child->map->mappings[i] = parent2->map->mappings[i];
    }
}

static void arithmetic_co(struct individual *child,
			  struct individual *parent1,
			  struct individual *parent2)
{
    struct ae_mapping *map = parent1->map;
    size_t i;
    unsigned int newpe;

    for (i = 0; i < map->ntasks; i++) {
	newpe = (parent1->map->mappings[i] + parent2->map->mappings[i]) % (map->arch->npes);
	ae_set_mapping(child->map, i, newpe);
    }
}

static void consensus_co(struct individual *child,
			 struct individual *parent1,
			 struct individual *parent2)
{
    struct ae_mapping *map = parent1->map;
    size_t i;

    for (i = 0; i < map->ntasks; i++) {
	if (parent1->map->mappings[i] == parent2->map->mappings[i])
	    child->map->mappings[i] = parent1->map->mappings[i];
	else
	    ae_set_mapping(child->map, i, ae_randi(0, map->arch->npes));
    }
}

static void consensus_2_co(struct individual *child,
			   struct individual *parent1,
			   struct individual *parent2)
{
    struct ae_mapping *map = parent1->map;
    size_t i;
    int x;
    int y;
    int newpe;

    for (i = 0; i < map->ntasks; i++) {
	x = parent1->map->mappings[i];
	y = parent2->map->mappings[i];
	if (x == y) {
	    child->map->mappings[i] = x;
	} else {
	    newpe = (x + y) % (map->arch->npes);
	    ae_set_mapping(child->map, i, newpe);
	}
    }
}

static struct individual *crossover(struct individual *parent1,
				    struct individual *parent2,
				    struct ga_parameters *p)
{
    struct individual *child;
    int recompute_fitness = 0;

    child = fork_individual(parent1, p);

    if (ae_randd(0, 1) < p->crossover_probability) {
	if (ae_randd(0, 1) < 0.5) {
	    struct individual *temp = parent1;
	    parent1 = parent2;
	    parent2 = temp;
	}

	p->crossoverbits(child, parent1, parent2);

	recompute_fitness = 1;
    }

    if (ae_randd(0, 1) < p->chromosome_mutation_probability) {
	p->mutation(child, p);
	recompute_fitness = 1;
    }

    if (recompute_fitness)
	child->fitness = fitness(child, p);

    return child;
}


static int fittest_first(const void *a, const void *b)
{
    struct individual *ia = * (struct individual **) a;
    struct individual *ib = * (struct individual **) b;

    if (ia->fitness > ib->fitness)
	return -1;
    else if (ia->fitness == ib->fitness)
	return 0;
    else
	return 1;
}


static struct individual *fork_individual(struct individual *individual,
					  struct ga_parameters *p)
{
    struct individual *clone;

    MALLOC_ARRAY(clone, 1);

    *clone = (struct individual) {
	.fitness = individual->fitness,
	.map = ae_fork_mapping(individual->map)};

    return clone;
}


static void free_individual(struct individual *individual)
{
    ae_free_mapping(individual->map);
    individual->map = NULL;
    free(individual);
}


static double gini_coefficient(double *selection_probability,
			       size_t population_size)
{
    double gini, sum, psum, y;
    size_t i;
    double n = population_size;

    sum = 0.0;
    psum = 0.0;

    for (i = 1; i <= population_size; i++) {
	y = selection_probability[population_size - i];
	psum += y;
	sum += (n + 1 - i) * y;
    }

    gini = (n + 1 - 2 * (sum / psum)) / n;

    return gini;
}


static struct individual *map_to_individual(struct ae_mapping *map,
					    struct ga_parameters *p)
{
    struct individual *individual;

    MALLOC_ARRAY(individual, 1);

    *individual = (struct individual) {.map = ae_fork_mapping(map)};

    individual->fitness = fitness(individual, p);

    return individual;
}


static void mutation(struct individual *individual, struct ga_parameters *p)
{
    unsigned int taskid;
    struct ae_mapping *map = individual->map;

    if (map->arch->npes == 1)
	return;

    for (taskid = 0; taskid < map->ntasks; taskid++) {
	if (ae_randd(0, 1) < p->gene_mutation_probability)
	    point_mutation(map, taskid);
    }
}


static void point_mutation(struct ae_mapping *map, unsigned int taskid)
{
    unsigned int newpe;

    if (map->arch->npes == 1)
	return;

    /* Get a random PE not same as the current one */
    newpe = ae_randi(0, map->arch->npes - 1);

    if (newpe >= map->mappings[taskid])
	newpe++;

    ae_set_mapping(map, taskid, newpe);
}


static size_t random_individual(double *selection_probability,
				size_t population_size)
{
    double sum = 0.0;
    size_t i;

    double x = ae_randd(0, 1);

    for (i = 0; i < population_size - 1; i++) {
	sum += selection_probability[i];

	if (x < sum)
	    break;
    }

    return i;
}


struct ae_mapping *ae_genetic_algorithm(struct ae_mapping *S0,
					struct ga_parameters *p)
{
    size_t generation, i;
    double best_fitness, fitness_sum;
    double *selection_probability;
    struct individual **population, **newpopulation;
    struct ae_mapping *S_best;
    double gini;

    assert(!ae_config.find_maximum);

    /* Initialize data structures */
    population = create_population(S0, p);

    MALLOC_ARRAY(newpopulation, p->population_size);

    MALLOC_ARRAY(selection_probability, p->population_size);

    /* START! */
    for (generation = 0; generation < p->max_generations; generation++) {

	qsort(population, p->population_size, sizeof(population[0]), fittest_first);

	fitness_sum = 0.0;
	best_fitness = 0.0;
	for (i = 0; i < p->population_size; i++) {
	    best_fitness = MAX(best_fitness, population[i]->fitness);
	    fitness_sum += population[i]->fitness;
	}

	for (i = 0; i < p->population_size; i++)
	    selection_probability[i] = population[i]->fitness / fitness_sum;

	gini = gini_coefficient(selection_probability, p->population_size);

	printf("best_ga_cost_so_far: %.9f %.3f %zd %lld %.3f\n",
	       fitness_to_cost(best_fitness),
	       p->initial_cost / fitness_to_cost(best_fitness),
	       generation, S0->result->evals, gini);

	/* Elitism: select elite automatically to the next generation
	   (the population is already in sorted order) */
	for (i = 0; i < p->elitism; i++)
	    newpopulation[i] = fork_individual(population[i], p);

	/* Competition (pair selection and breeding) */
	for (; i < p->population_size; i++) {
	    size_t ind1, ind2;

	    ind1 = random_individual(selection_probability, p->population_size);
	    ind2 = random_individual(selection_probability, p->population_size);

	    newpopulation[i] = p->crossover(population[ind1], population[ind2], p);
	}

	/* copy results to the next generation, kill this generation */
	for (i = 0; i < p->population_size; i++) {
	    free_individual(population[i]);
	    population[i] = newpopulation[i];
	}
    }

    /* Find the best individual */
    qsort(population, p->population_size, sizeof(population[0]), fittest_first);
    S_best = ae_fork_mapping(population[0]->map);

    for (i = 0; i < p->population_size; i++)
	free_individual(population[i]);

    free(selection_probability);
    free(population);
    free(newpopulation);

    return S_best;
}


struct ga_parameters *ae_ga_read_parameters(FILE *f)
{
    struct ga_parameters *p;
    int i;
    char *s;

    struct valuepair {
	char *name;
	double value;
	int initialized;
    };

    struct valuepair pars[] = {
	{.name = "max_generations",                 .value = 1000},
	{.name = "population_size",                 .value = 100},
	{.name = "elitism",                         .value = 10},
	{.name = "crossover_probability",           .value = 1.0},
	{.name = "chromosome_mutation_probability", .value = 1.0},
	{.name = "gene_mutation_probability",       .value = 0.01},
	{.name = "crossover_method",                .value = (double) UNIFORM_CO},
	{.name = NULL}};

    int nparameters = sizeof(pars) / sizeof(struct valuepair) - 1;

    CALLOC_ARRAY(p, 1);

    /* I should really write a generic handler for these situations where
       we have set of named operands for an algorithm. We could re-use it
       for all different algorithms. struct valuepair is lame. */

    while (1) {
	s = ae_get_word(f);

	if (strcmp(s, "end_opt_parameters") == 0)
	    break;

	for (i = 0; pars[i].name != NULL; i++) {
	    if (strcmp(s, pars[i].name) != 0)
		continue;
	    
	    pars[i].value = ae_get_double(f);
	    pars[i].initialized = 1;
	}

	free(s);
    }

    for (i = 0; pars[i].name != NULL; i++) {
	if (!pars[i].initialized)
	    printf("warning: %s not initialized\n", pars[i].name);
    }

    /* Command line parameter format is:
       (a, b, c, d, e, f, g), where
                                 a = max_generations,
                                 b = population_size,
				 c = elitism,
				 d = crossover_probability,
				 e = chromosome_mutation_probability,
				 f = gene_mutation_probability,
                                 g = crossover_method */

    s = ae_config.cmdline_optimization_parameter;

    if (s) {
	for (i = 0;; i++) {
	    char *end;

	    pars[i].value = strtod(s, &end);
	    if (i < (nparameters - 1)) {
		assert(*end == ',');
		s = end + 1;
	    } else {
		assert(*end == 0);
		break;
	    }
	}
    }

    p->max_generations = pars[0].value;
    p->population_size = pars[1].value;
    p->elitism = pars[2].value;
    p->crossover_probability = pars[3].value;
    p->chromosome_mutation_probability = pars[4].value;
    p->gene_mutation_probability = pars[5].value;
    p->crossover_method = (enum crossover_method) pars[6].value;

    assert(p->max_generations > 0);
    assert(p->population_size > 0);
    assert(p->elitism >= 0 && p->elitism <= p->population_size);
    assert(p->crossover_probability >= 0.0 && p->crossover_probability <= 1.0);
    assert(p->chromosome_mutation_probability >= 0.0 && p->chromosome_mutation_probability <= 1.0);
    assert(p->gene_mutation_probability >= 0.0 && p->gene_mutation_probability <= 1.0);

    /* Maybe we should get rid of these, since we only have one function
       for each */
    p->mutation = mutation;
    p->crossover = crossover;
    switch (p->crossover_method) {
    case SINGLE_POINT_CO:
	p->crossoverbits = single_point_co;
	break;
    case TWO_POINT_CO:
	p->crossoverbits = two_point_co;
	break;
    case UNIFORM_CO:
	p->crossoverbits = uniform_co;
	break;
    case ARITHMETIC_CO:
	p->crossoverbits = arithmetic_co;
	break;
    case CONSENSUS_CO:
	p->crossoverbits = consensus_co;
	break;
    case CONSENSUS_2_CO:
	p->crossoverbits = consensus_2_co;
	break;
    default:
	ae_err("Invalid crossover enum: %d\n", p->crossover_method);
    }

    printf("GA parameters:\n"
	   "max_generations: %zu\n"
	   "population_size: %zu\n"
	   "elitism: %zu\n"
	   "crossover_probability: %.6f\n"
	   "crossover_method: %d\n"
	   "chromosome_mutation_probability: %.6f\n"
	   "gene_mutation_probability: %.6f\n",
	   p->max_generations, p->population_size, p->elitism,
	   p->crossover_probability, p->crossover_method,
	   p->chromosome_mutation_probability, p->gene_mutation_probability);

    return p;
}
