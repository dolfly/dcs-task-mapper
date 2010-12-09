/*
 * vplist module (including its header) is in Public Domain. No copyrights
 * are claimed. The original author is Heikki Orsila <heikki.orsila@iki.fi>.
 */
#include <assert.h>
#include <stdlib.h>

#include "vplist.h"


/* Make a new tail. O(n) operation. Returns 0 on success, -1 on failure. */
int vplist_append(struct vplist *v, void *item)
{
	assert(item != NULL);

	while (v->next != NULL)
		v = v->next;

	v->next = malloc(sizeof v[0]);
	if (v->next == NULL)
		return -1;

	*v->next = (struct vplist) {.item = item};

	return 0;
}


/* Allocate a new list stub. */
struct vplist *vplist_create(void)
{
	struct vplist *v = malloc(sizeof v[0]);
	if (v == NULL)
		return NULL;
	*v = (struct vplist) {.next = NULL};
	return v;
}


static void vplist_internal_free(struct vplist *v, int freeitems)
{
	struct vplist *next;

	while (v != NULL) {
		next = v->next;
		v->next = NULL;
		if (freeitems)
			free(v->item);
		v->item = NULL;
		free(v);
		v = next;
	}
}

void vplist_free(struct vplist *v)
{
	/* Remove all items */
	vplist_internal_free(v, 0);
}


/*
 * Free all items in the list. O(n) operation. Note, doesn't unallocate
 * the list stub. It is allowed to append to the list after this operation.
 */
void vplist_free_items(struct vplist *v)
{
	vplist_internal_free(v->next, 1);
	v->next = NULL;
}

/*
 * Get item i, where i == 0 is the first item. If pop != 0, remove the list
 * item, otherwise just return the item. Returns NULL if the list is too
 * short (i >= vplist_len(v));
 */
static void *vplist_internal_get(struct vplist *v, size_t i, int pop)
{
	struct vplist *next;
	void *item;

	while (v->next != NULL && i > 0) {
		i--;
		v = v->next;
	}

	if (v->next == NULL)
		return NULL;

	/* Item to be returned and possibly freed */
	next = v->next;
	item = next->item;
	assert(item != NULL);

	if (!pop)
		return item;

	v->next = next->next;

	/* Prevent damage by removing pointers from the removed item */
	next->next = NULL;
	next->item = NULL;

	free(next);
	return item;
}


/*
 * Return element i from the list (i == 0 is the head). Return NULL if
 * i is too high (i >= vplist_len()). O(n) operation.
 */
void *vplist_get(const struct vplist *v, size_t i)
{
	return vplist_internal_get((struct vplist *) v, i, 0);
}


/* Init an existing list head */
void vplist_init(struct vplist *v)
{
	v->next = NULL;
	v->item = NULL;
}


/* Returns 1 if the list is empty, otherwise 0 */
int vplist_is_empty(const struct vplist *v)
{
	return v->next == NULL;
}


/* Return the number of elements in list. O(n) operation. */
size_t vplist_len(const struct vplist *v)
{
	size_t l = 0;

	while (v->next != NULL) {
		v = v->next;
		l++;
	}

	return l;
}


/* Pop item i. O(n) operation. */
void *vplist_pop(struct vplist *v, size_t i)
{
	return vplist_internal_get(v, i, 1);
}


/* Pop head of the list. O(1) operation. */
void *vplist_pop_head(struct vplist *v)
{
	return vplist_internal_get(v, 0, 1);
}


/* Pop tail of the list. O(n) operation. */
void *vplist_pop_tail(struct vplist *v)
{
	if (v->next == NULL)
		return NULL;

	while (v->next->next != NULL)
		v = v->next;

	return vplist_internal_get(v, 0, 1);
}


/* Remove node that matches item in the list. Returns 0 on success, -1 on
 * failure. */
int vplist_remove_item(struct vplist *v, void *item)
{
	void *removeditem;

	/* Find the node that precedes the node that is being searched for */
	while (v->next != NULL && v->next->item != item)
		v = v->next;

	if (v->next == NULL)
		return -1;

	removeditem = vplist_internal_get(v, 0, 1);
	assert(removeditem == item);

	return 0;
}
