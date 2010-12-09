#ifndef PERMUTATION_H
#define PERMUTATION_H

struct permutation {
	/* internal state */
	unsigned int n;
	unsigned int *permutation;
	unsigned int *pivots;

	/* storage for work */
	unsigned int *tmp;
};

void permutation_free(struct permutation *p);
int permutation_init(struct permutation *p, unsigned int n);
int permutation_next(struct permutation *p);
void permutation_reset(struct permutation *p);
int permutation_set(struct permutation *p, unsigned int *x);

#endif
