#ifndef _AE_AREXBASIC_H_
#define _AE_AREXBASIC_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

void ae_add_resource(int *n, int *nallocated, void ***resources, void *resource);
void *ae_fork_memory(void *src, size_t size);
double ae_randd(double a, double b);

/* Returns an integer x that satisfies a <= x < b */
int ae_randi(int a, int b);

void ae_random_cards(int *cards, int n, int maximum);

/* int64_t ae_randi64(int64_t a, int64_t b); */

#define ae_err(fmt, args...) do { fprintf(stderr, "%s:%d: %s: " fmt, __FILE__, __LINE__, __func__, ## args); abort(); } while(0)

#define CALLOC_ARRAY(array, n) do { if (((array) = calloc((n), sizeof((array)[0]))) == NULL) ae_err("not enough memory\n"); } while(0)
#define MALLOC_ARRAY(array, n) do { if (((array) = malloc(sizeof((array)[0]) * (n))) == NULL) ae_err("not enough memory\n"); } while(0)
#define REALLOC_ARRAY(array, n) do { if (((array) = realloc((array), sizeof((array)[0]) * (n))) == NULL) ae_err("not enough memory\n"); } while(0)

#define MAX(x, y) ((x) >= (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#endif
