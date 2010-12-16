#ifndef _AE_INPUT_H_
#define _AE_INPUT_H_

#include "arextypes.h"
#include "vplist.h"
#include <stdio.h>

struct arex_parameter {
	char *name;
	char *value;
};

struct arex_config {
	int fast_premapping;
	int find_maximum; /* find maximum instead of minimum, if non-zero */
	char *cmdline_optimization_parameter;
	char *arbitration_policy;
	char *ic_priorities;
	struct vplist parameters;
};

extern struct arex_config ae_config;

void ae_config_append_parameter(const char *name, const char *value);
int ae_config_get_int(int *success, const char *name);
long long ae_config_get_long_long(int *success, const char *name);
size_t ae_config_get_size_t(int *success, const char *name);
double ae_get_double(FILE *f);
int ae_get_int(FILE *f);
long long ae_get_long_long(FILE *f);
unsigned int ae_get_uint(FILE *f);
char *ae_get_word(FILE *f);
int ae_match_alternatives(char **alts, FILE *f);
void ae_match_word(const char *str, FILE *f);
struct ae_mapping *ae_read_input(FILE *f);

#endif
