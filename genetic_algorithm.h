#ifndef _AE_GENETIC_ALGORITHM_
#define _AE_GENETIC_ALGORITHM_

#include <stdio.h>

#include "arextypes.h"


struct individual {
	double fitness;
	struct ae_mapping *map;
};

struct ga_parameters {
	size_t max_generations;
	size_t stop_generations;

	size_t population_size;
	size_t elitism;
	size_t discrimination;

	double crossover_probability;
	const char *crossover_method;
	double chromosome_mutation_probability;
	double gene_mutation_probability;

	double initial_cost;

	struct individual * (*crossover)(struct individual *parent1,
					 struct individual *parent2,
					 struct ga_parameters *p);
	void (*crossoverbits)(struct individual *child,
			      struct individual *parent1,
			      struct individual *parent2);
	void (*mutation)(struct individual *individual, struct ga_parameters *p);
	double (*objective)(struct ae_mapping *map);
};

struct ae_mapping *ae_genetic_algorithm(struct ae_mapping *S0,
					struct ga_parameters *p);
struct ga_parameters *ae_ga_read_parameters(FILE *f);

#endif
