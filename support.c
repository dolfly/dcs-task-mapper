#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "support.h"

char *xstrdup(const char *s)
{
	char *t = strdup(s);
	if (t == NULL) {
		fprintf(stderr, "Can not strdup(s)\n");
		abort();
	}
	return t;
}
