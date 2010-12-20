#ifndef _AE_DATASTRUCTURES_H_
#define _AE_DATASTRUCTURES_H_

#include <string.h>
#include <stdint.h>

struct intlistarray {
	int n;
	int *nentries;
	int *nentriesallocated;
	int **entries;
};

struct ae_heap {
	int (*compare)(void *a, void *b);
	int unit;
	int n;
	int nallocated;
	int8_t *temp;
	int8_t *heap;
};


void ae_add_to_ila(struct intlistarray *ila, int n, int x);
void ae_free_ila(struct intlistarray *ila);
void ae_init_ila(struct intlistarray *ila, int n);
void ae_heap_flush(struct ae_heap *h);
void ae_heap_free(struct ae_heap *h);
void ae_heap_init(struct ae_heap *h, int (*compare)(void *, void *), int unit);
void ae_heap_insert(struct ae_heap *h, void *key);
void ae_heap_ify(struct ae_heap *h, int i);
void ae_heap_extract_max(void *res, struct ae_heap *h);
void ae_rm_from_ila(struct intlistarray *ila, int n, int x);


#define AE_HEAP_COMPARE(h, a, b) ((h)->compare((a), (b)))
#define AE_HEAP_PTR(h, i) (((h)->heap + (i) * ((h)->unit)))
#define AE_HEAP_PARENT(x) ((x) / 2)
#define AE_HEAP_COPY(h, i, j) memmove(AE_HEAP_PTR((h), (i)), AE_HEAP_PTR((h), (j)), (h)->unit)
#define AE_HEAP_GET(h, dst, i) memcpy((dst), AE_HEAP_PTR((h), (i)), (h)->unit)
#define AE_HEAP_PUT(h, i, src) memcpy(AE_HEAP_PTR((h), (i)), (src), (h)->unit)

#endif
