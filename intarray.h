#ifndef _INTARRAY_H_
#define _INTARRAY_H_

#include <stdlib.h>
#include <assert.h>


struct intarray {
	int created;
	size_t head;
	size_t tail;
	size_t allocated;
	int *l;
};


/* Don't touch */
void intarray_grow(struct intarray *v);

/* Add one element to the end of the list. Asymptotic: amortically O(1) */
static inline void intarray_append(struct intarray *v, int x)
{
	if (v->tail == v->allocated)
		intarray_grow(v);

	v->l[v->tail++] = x;
}

/* Return void ** to the beginning of valid array inside the list. This
   is the same as &v->l[v->head]. Items before this are invalid. Asymptotic:
   trivially O(1) */
static inline int *intarray_array(struct intarray *v)
{
	return &v->l[v->head];
}

/* Same as intarray_search, but faster. intarray_sort() must be used before using
   this. Also, the function must be given the same sort function as
   intarray_sort() was given. The function returns an index of the found
   element, or -1 if not found. See intarray_get() to obtain the element based
   on the index. Asymptotic: O(log N) */
ssize_t intarray_bsearch(int key, struct intarray *v,
			 int (*compar)(const void *, const void *));

/* Create a new data structure */
struct intarray *intarray_create(size_t initial_length);

/* Remove all elements from the list */
void intarray_flush(struct intarray *v);

/* Free memory of the data structure. Asymptotic: that of free(array) */
void intarray_free(struct intarray *v);

/* Get one element by index in range [0, N), where N = intarray_len(v).
   Asymptotic: trivially O(1) */
static inline int intarray_get(struct intarray *v, size_t i)
{
	assert(i < (v->tail - v->head));
	return v->l[v->head + i];
}

void intarray_initialize(struct intarray *v, size_t initial_length);

/* Return the number of valid items in the list. Asymptotic: trivially O(1) */
static inline size_t intarray_len(struct intarray *v)
{
	return v->tail - v->head;
}

/* Remove and return the first integer. Asymptotic: amortically O(1) */
int intarray_pop_head(struct intarray *v);

/* Remove and return the last integer. Asymptotic: amortically O(1) */
int intarray_pop_tail(struct intarray *v);

/*
 * Remove index i from the list. Note, if you use this function, you may
 * not assume the order of entries in the array is still the same. In other
 * words, do not use it as a lifo or fifo in this case.
 * Asymptotic: that of intarray_pop_tail().
 */
void intarray_remove(struct intarray *v, size_t i);

/* Find an integer key from the list, returns the index of the element if
   found, otherwise returns -1. See intarray_get() to obtain the element
   based on the index. Asymptotic: O(n) */
ssize_t intarray_search(int key, struct intarray *v);

/* Sort the list. The given compare function must act like the one given for
   qsort(). The compare function gets two ints. A typical comparison
   function might look like this:
     int compare(const void *a, const void *b)
     {
        int x = * (int *) a;
        int y = * (int *) b;
        if (x < y)
	  return -1;
	if (y < x)
	  return 1;
	return 0;
     }

   Note, any modification to the list may unsort the list (adding and removing
   items). If intarray_bsearch() is used, the list
   must always be re-sorted after adding or removing elements. Asymptotic:
   that of qsort() */
void intarray_sort(struct intarray *v, int (*compar)(const void *, const void *));

#endif
