#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "datastructures.h"
#include "arexbasic.h"


void ae_init_ila(struct intlistarray *ila, int n)
{
  assert(n > 0);
  memset(ila, 0, sizeof(*ila));
  ila->n = n;
  ila->nentries = calloc(1, n * sizeof(ila->nentries[0]));
  ila->nentriesallocated = calloc(1, n * sizeof(ila->nentriesallocated[0]));
  ila->entries = calloc(1, n * sizeof(ila->entries[0]));
}


void ae_add_to_ila(struct intlistarray *ila, int n, int x)
{
  assert(n >= 0 && n < ila->n);

  if (ila->nentries[n] == ila->nentriesallocated[n]) {
    if (ila->nentriesallocated[n] == 0)
      ila->nentriesallocated[n] = 4;
    else
      ila->nentriesallocated[n] *= 2;
    ila->entries[n] = realloc(ila->entries[n], sizeof(ila->entries[0][0]) * ila->nentriesallocated[n]);
    if (ila->entries[n] == NULL)
      ae_err("ae_add_to_ila: no memory\n");
  }

  ila->entries[n][ila->nentries[n]] = x;
  ila->nentries[n]++;
}

void ae_free_ila(struct intlistarray *ila)
{
  int i;
  for (i = 0; i < ila->n; i++) {
    free(ila->entries[i]);
    ila->entries[i] = NULL;
  }
  free(ila->entries);
  free(ila->nentries);
  free(ila->nentriesallocated);
  ila->entries = NULL;
  ila->nentries = NULL;
  ila->nentriesallocated = NULL;
}


void ae_heap_init(struct ae_heap *h, int (*compare)(void *, void *), int unit)
{
  h->compare = compare;
  h->unit = unit;
  h->n = 0;
  h->nallocated = 1;
  h->heap = calloc(1, unit * (h->nallocated + 1));
  if (h->heap == NULL) {
    fprintf(stderr, "not enough memory for heap\n");
    exit(-1);
  }
  if ((h->temp = malloc(unit)) == NULL) {
    fprintf(stderr, "%s: not enough memory for temp\n", __func__);
    exit(-1);
  }
}


void ae_heap_flush(struct ae_heap *h)
{
  h->n = 0;
}


void ae_heap_free(struct ae_heap *h)
{
  free(h->heap);
  h->heap = NULL;
  free(h->temp);
  h->temp = NULL;
}


void ae_heap_insert(struct ae_heap *h, void *key)
{
  int x;
  assert(h->n >= 0);
  h->n++;

  if (h->n > h->nallocated) {
    h->nallocated = h->nallocated * 2 + 1;
    h->heap = realloc(h->heap, (h->nallocated + 1) * h->unit);
    if (h->heap == NULL)
      fprintf(stderr, "%s: no memory\n", __func__);
  }

  x = h->n;
  while (x > 1 && AE_HEAP_COMPARE(h, AE_HEAP_PTR(h, AE_HEAP_PARENT(x)), key) == -1) {
    AE_HEAP_COPY(h, x, AE_HEAP_PARENT(x));
    x = AE_HEAP_PARENT(x);
  }
  AE_HEAP_PUT(h, x, key);
}


void ae_heap_ify(struct ae_heap *h, int i)
{
  int L, R, largest;
  if (h->n == 0)
    return;

  assert(i > 0 && i <= h->n);

  while (i) {
    L = 2 * i;
    R = 2 * i + 1;
    if (L <= h->n && h->compare(AE_HEAP_PTR(h, L), AE_HEAP_PTR(h, i)) == 1)
      largest = L;
    else
      largest = i;
    if (R <= h->n && h->compare(AE_HEAP_PTR(h, R), AE_HEAP_PTR(h, largest)) == 1)
      largest = R;
    if (largest != i) {
      /* swap 'i' and 'largest' */
      AE_HEAP_GET(h, h->temp, i);
      AE_HEAP_COPY(h, i, largest);
      AE_HEAP_PUT(h, largest, h->temp);
      i = largest;
    } else {
      i = 0;
    }
  }
}


void ae_heap_extract_max(void *res, struct ae_heap *h)
{
  if (h->n <= 0) {
    fprintf(stderr, "%s: h->n == %d\n", __func__, h->n);
    fflush(stderr);
    * (int *) NULL = 0;
  }
  AE_HEAP_GET(h, res, 1);
  AE_HEAP_COPY(h, 1, h->n);
  h->n--;
  ae_heap_ify(h, 1);
}


void ae_rm_from_ila(struct intlistarray *ila, int n, int x)
{
  int *l;
  int i;
  int nentries;

  assert(n >= 0 && n < ila->n);

  if ((l = ila->entries[n]) == NULL)
    ae_err("int %d not found from ila\n", x);

  nentries = ila->nentries[n];

  for (i = 0; i < nentries; i++) {
    if (l[i] == x) {
      /* rotate list left so that element on position 'i' disappears */
      i++;
      for (; i < nentries; i++)
	l[i - 1] = l[i];
      ila->nentries[n]--;
      return;
    }
  }
  ae_err("int %d not found from ila\n", x);
}
