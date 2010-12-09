#ifndef _VPLIST_H_
#define _VPLIST_H_

#include <stdio.h>

struct vplist;

struct vplist {
	struct vplist *next;
	void *item;
};

/* VPLIST_INITIALIZER can be used to initialize an instance of struct vplist
 * -> no need to call vplist_init() */

#define VPLIST_INITIALIZER (struct vplist) {.next = NULL}

#define VPLIST_FOR_EACH(ltemp, l) for ((ltemp) = (l)->next; (ltemp) != NULL; (ltemp) = (ltemp)->next)

int vplist_append(struct vplist *v, void *item);
struct vplist *vplist_create(void);
void vplist_free(struct vplist *v);
void vplist_free_items(struct vplist *v);
void *vplist_get(const struct vplist *v, size_t i);
void vplist_init(struct vplist *v);
int vplist_is_empty(const struct vplist *v); /* faster than vplist_len() */
size_t vplist_len(const struct vplist *v);
void *vplist_pop(struct vplist *v, size_t i);
void *vplist_pop_head(struct vplist *v);
void *vplist_pop_tail(struct vplist *v);
int vplist_remove_item(struct vplist *v, void *item);

#endif
