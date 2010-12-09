/*  Amortized O(1) dynamic list written by
    Heikki Orsila <heikki.orsila@iki.fi>. The code is in public domain, so
    it may be used without restrictions for anything.

    RESTRICTIONS: sizeof(size_t) must be same as sizeof(void *). In other
    words, the whole memory space must be indexable with a size_t. If this
    does not hold, do not use this data structure.
*/

#include <stdio.h>
#include <string.h>

#include "intarray.h"

#define BASIC_LENGTH 5


static void shrink_intarray(struct intarray *v, size_t newsize)
{
	size_t ncopied = v->tail - v->head;
	int *newl;
	if (newsize >= v->allocated) {
		fprintf(stderr, "intarray not shrinked.\n");
		return;
	}

	memmove(v->l, intarray_array(v), ncopied * sizeof(v->l[0]));
	v->head = 0;
	v->tail = ncopied;
	v->allocated = newsize;
	newl = realloc(v->l, v->allocated * sizeof(v->l[0]));
	if (newl == NULL) {
		fprintf(stderr, "Not enough memory for shrinking intarray.\n");
		abort();
	}
	v->l = newl;
}


ssize_t intarray_bsearch(int key, struct intarray *v,
			 int (*compar)(const void *, const void *))
{
	void *res;
	int *array = intarray_array(v);
	size_t n = intarray_len(v);

	res = bsearch(&key, array, n, sizeof(v->l[0]), compar);
	if (res == NULL)
		return -1;

	return (ssize_t) (((size_t) res - (size_t) array) / sizeof(int));
}


void intarray_grow(struct intarray *v)
{
	size_t newsize = v->allocated * 2;
	int *newl;

	if (newsize == 0)
		newsize = BASIC_LENGTH;

	newl = realloc(v->l, newsize * sizeof(v->l[0]));
	if (newl == NULL) {
		fprintf(stderr, "Not enough memory for growing intarray.\n");
		abort();
	}

	v->l = newl;
	v->allocated = newsize;
}


struct intarray *intarray_create(size_t initial_length)
{
	struct intarray *v;

	v = malloc(sizeof(*v));
	if (v == NULL) {
		fprintf(stderr, "No memory for intarray.\n");
		abort();
	}

	intarray_initialize(v, initial_length);

	v->created = 1;

	return v;
}

void intarray_initialize(struct intarray *v, size_t initial_length)
{
	memset(v, 0, sizeof(*v));

	if (initial_length == 0)
		initial_length = BASIC_LENGTH;

	v->allocated = initial_length;

	v->l = malloc(v->allocated * sizeof(v->l[0]));
	if (v->l == NULL) {
		fprintf(stderr, "Can not create a intarray.\n");
		abort();
	}
}


void intarray_flush(struct intarray *v)
{
	v->head = 0;
	v->tail = 0;

	if (v->allocated >= (2 * BASIC_LENGTH))
		shrink_intarray(v, BASIC_LENGTH);
}


void intarray_free(struct intarray *v)
{
	int created = v->created;

	free(v->l);
	memset(v, 0, sizeof(*v));

	if (created)
		free(v);
}


int intarray_pop_head(struct intarray *v)
{
	int item;

	if (v->head == v->tail) {
		fprintf(stderr, "Error: can not pop head from an empty intarray.\n");
		abort();
	}

	item = v->l[v->head++];

	/* If 3/4 of a list is unused, free half the list */
	if (v->allocated >= BASIC_LENGTH && v->head >= ((v->allocated / 4) * 3))
		shrink_intarray(v, v->allocated / 2);

	return item;
}


int intarray_pop_tail(struct intarray *v)
{
	int item;

	if (v->head == v->tail) {
		fprintf(stderr, "Error: can not pop tail from an empty intarray.\n");
		abort();
	}

	item = v->l[v->tail--];

	/* If 3/4 of a list is unused, free half the list */
	if (v->allocated >= BASIC_LENGTH && v->tail < (v->allocated / 4))
		shrink_intarray(v, v->allocated / 2);

	return item;
}


void intarray_remove(struct intarray *v, size_t i)
{
	size_t n = intarray_len(v);
	int *array = intarray_array(v);

	assert(i < n);

	/* Warning, changes the order in the array */
	array[i] = array[n - 1];

	intarray_pop_tail(v);
}


ssize_t intarray_search(int key, struct intarray *v)
{
	size_t i;
	size_t n = intarray_len(v);
	int *array = intarray_array(v);

	for (i = 0; i < n; i++) {
		if (array[i] == key)
			return (ssize_t) i;
	}

	return -1;
}


void intarray_sort(struct intarray *v,
		   int (*compar)(const void *, const void *))
{
	size_t n = intarray_len(v);

	if (n <= 1)
		return;

	qsort(intarray_array(v), n, sizeof v->l[0], compar);
}
