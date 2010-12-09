#include "permutation.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void basic_permutation(unsigned int *x, unsigned int n)
{
	unsigned int i;
	for (i = 0; i < n; i++)
		x[i] = i;
}

int permutation_init(struct permutation *p, unsigned int n)
{
	if (n == 0)
		return -1;

	*p = (struct permutation) {.n = n};
	p->permutation = malloc(sizeof(p->permutation[0]) * n);
	p->pivots = malloc(sizeof(p->pivots[0]) * n);
	p->tmp = malloc(sizeof(p->pivots[0]) * n);

	if (p->permutation == NULL || p->pivots == NULL || p->tmp == NULL) {
		permutation_free(p);
		return -1;
	}

	permutation_reset(p);
	return 0;
}

void permutation_free(struct permutation *p)
{
	/* permutation poison */
	if (p->permutation)
		memset(p->permutation, -1, sizeof(p->permutation[0]) * p->n);
	free(p->permutation);
	p->permutation = NULL;

	free(p->pivots);
	p->pivots = NULL;

	free(p->tmp);
	p->tmp = NULL;
}

int permutation_next(struct permutation *p)
{
	unsigned int i;
	unsigned int j;
	unsigned int choices;
	unsigned int pivotval;
	unsigned int pivotind;
	unsigned int len;

	i = p->n - 1;
	p->pivots[i] = 1;

	while (1) {
		choices = p->n - i;
		if (p->pivots[i] == choices) {
			if (i == 0)
				return -1;
			i--;
			p->pivots[i]++;
			for (j = i + 1; j < p->n; j++)
				p->pivots[j] = 0;
			continue;
		}
		break;
	}

	basic_permutation(p->tmp, p->n);

	for (i = 0; i < p->n; i++) {
		pivotind = p->pivots[i];
		pivotval = p->tmp[pivotind];
		p->permutation[i] = pivotval;

		if (i == (p->n - 1))
			break;

		len = p->n - i; /* total number of values in p->tmp */
		assert(len >= (pivotind + 1));

		/* Remove pivotind entry from p->tmp array */
		memmove(&p->tmp[pivotind], &p->tmp[pivotind + 1], sizeof(p->tmp[0]) * (len - pivotind - 1));
	}

	return 0;
}

void permutation_reset(struct permutation *p)
{
	basic_permutation(p->permutation, p->n);

	memset(p->pivots, 0, sizeof(p->pivots[0]) * p->n);
}

/* Returns 0 if permutation x is valid, otherwise -1 */
int permutation_set(struct permutation *p, unsigned int *x)
{
	unsigned int i;
	unsigned int j;
	unsigned int choices;

	/* Check that permutation x is valid */
	memset(p->tmp, 0, sizeof(p->tmp[0]) * p->n);
	for (i = 0; i < p->n; i++) {
		if (x[i] >= p->n || p->tmp[x[i]])
			return -1;
		p->tmp[x[i]] = 1;
	}

	memcpy(p->permutation, x, sizeof(p->permutation[0]) * p->n);

	basic_permutation(p->tmp, p->n);

	for (i = 0; i < p->n; i++) {
		choices = p->n - i;
		for (j = 0; j < choices; j++) {
			if (p->tmp[j] == p->permutation[i])
				break;
		}
		assert(j < choices);
		p->pivots[i] = j;

		/* Remove pivot entry from p->tmp array */
		memmove(&p->tmp[j], &p->tmp[j + 1], sizeof(p->tmp[0]) * (choices - j - 1));
	}

	return 0;
}
