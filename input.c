#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "input.h"
#include "arexbasic.h"
#include "pe.h"
#include "arextypes.h"
#include "arch.h"
#include "interconnect.h"
#include "task.h"
#include "mapping.h"
#include "optimization.h"
#include "kpn.h"
#include "support.h"

struct arex_config ae_config;

static char *get_parameter(const char *name)
{
	struct vplist *node;
	struct arex_parameter *par;

	VPLIST_FOR_EACH(node, &ae_config.parameters) {
		par = node->item;
		if (strcmp(par->name, name) == 0)
			return par->value;
	}

	return NULL;
}

void ae_config_append_parameter(const char *name, const char *value)
{
	struct arex_parameter *par;
	fprintf(stderr, "append parameter: %s = %s\n", name, value);
	MALLOC_ARRAY(par, 1);
	par->name = xstrdup(name);
	par->value = xstrdup(value);
	vplist_append(&ae_config.parameters, par);
}

long long ae_config_get_long_long(int *success, const char *name)
{
	void *value;
	long long ll;
	char *endptr;

	if (success)
		*success = 0;

	value = get_parameter(name);
	if (value == NULL)
		return 0;

	ll = strtoll(value, &endptr, 10);
	if (*endptr != 0)
		return 0;
	if (success)
		*success = 1;
	return ll;
}

int ae_config_get_int(int *success, const char *name)
{
	long long ll = ae_config_get_long_long(success, name);
	if (success) {
		if (*success == 0)
			return 0;
		*success = 0;
	}
	if (ll < INT_MIN || ll > INT_MAX)
		return 0;
	if (success)
		*success = 1;
	return (int) ll;
}

size_t ae_config_get_size_t(int *success, const char *name)
{
	long long ll = ae_config_get_long_long(success, name);
	if (success) {
		if (*success == 0)
			return 0;
		*success = 0;
	}
	if (ll < 0 || ll > ((size_t) ~0))
		return 0;
	if (success)
		*success = 1;
	return (int) ll;
}

double ae_get_double(FILE *f)
{
  int ret;
  double res;
  ret = fscanf(f, "%lf", &res);
  if (ret != 1)
    ae_err("no input when trying to get double\n"); 
  return res;
}


int ae_get_int(FILE *f)
{
  int res, ret;
  ret = fscanf(f, "%d", &res);
  if (ret != 1)
    ae_err("no input when trying to get int\n");
  return res;
}

long long ae_get_long_long(FILE *f)
{
	long long res;
	char *end;
	char buf[256];
	if (fscanf(f, "%s", buf) != 1)
		ae_err("no input when trying to get long long\n");
	res = strtoll(buf, &end, 10);
	if (*end != 0 || res == LLONG_MIN || res == LLONG_MAX)
		ae_err("Invalid long long integer: %s\n", buf);
	return res;
}

unsigned int ae_get_uint(FILE *f)
{
  int ret;
  unsigned int res;
  ret = fscanf(f, "%u", &res);
  if (ret != 1)
    ae_err("no input when trying to get unsigned int\n");
  return res;
}

char *ae_get_word(FILE *f)
{
  int ret;
  char buf[256];
  char *s;

  ret = fscanf(f, "%255s", buf);

  if (ret != 1)
    ae_err("no input when trying to get int\n");

  s = strdup(buf);
  if (s == NULL)
    ae_err("Not enough memory when reading %s\n", buf);

  return s;
}


/* matches alternative inputs from f. returns index of the match to the
   'alts' array. if there is no match due to EOF, -2 is returned. if there
   is no match otherwise, -1 is returned */
int ae_match_alternatives(char **alts, FILE *f)
{
  int res, ret;
  char buf[256];
  ret = fscanf(f, "%255s", buf);
  if (ret != 1) {
    if (feof(f))
      return -2;
    return -1;
  }
  buf[255] = 0;
  res = 0;
  while (*alts) {
    if (strcmp(*alts, buf) == 0)
      break;
    res++;
    alts++;
  }
  if (*alts == NULL)
    ae_err("no match on alternatives (got %s)\n", buf);
  return res;
}


void ae_match_word(const char *str, FILE *f)
{
  int ret;
  char buf[256];
  ret = fscanf(f, "%255s", buf);
  if (ret != 1)
    ae_err("no input when trying to match %s\n", str);
  buf[255] = 0;
  if (strcmp(str, buf))
    ae_err("%s not matched (got %s)\n", str, buf);
}


static void match_architecture(struct ae_arch *arch, FILE *f)
{
  char *arch_categories[] = {"processing_element_list",
			     "interconnect_list", NULL};

  int ics = -1;
  int pes = -1;
  int ret;
  int i;
  struct ae_pe *pe;
  struct ae_interconnect *ic;
  double totalarea = 0.0;
  char *icpri = ae_config.ic_priorities;

  while (ics == -1 || pes == -1) {

    ret = ae_match_alternatives(arch_categories, f);

    switch (ret) {
    case 0:
      assert(pes == -1);
      pes = ae_get_int(f);
      assert(pes > 0);

      for (i = 0; i < pes; i++) {
	ae_match_word("processing_element", f);
	ae_add_pe_string(arch, f);

	pe = arch->pes[i];

	if (icpri != NULL && i < strlen(icpri)) {
		pe->ic_initial_priority = icpri[i] - '0';
		assert(pe->ic_initial_priority >= 0);
	}

	totalarea += pe->area;

	printf("pe: id %d freq %lld perf %e area %e\n",
	       pe->id, pe->freq, pe->performance_factor, pe->area);
      }

      break;

    case 1:
      assert(ics == -1);
      ics = ae_get_int(f);
      assert(ics > 0);

      for (i = 0; i < ics; i++) {
	ae_match_word("interconnect", f);
	ae_add_ic_string(arch, f);

	ic = arch->ics[i];

	printf("interconnect: id %d freq %lld area %e width %d latency %d policy %d\n",
	       ic->id, ic->freq, ic->area, ic->width, ic->latency, ic->policy);
      }

      for (i = 0; i < ics; i++) {
	if (arch->ics[i]->freq != arch->ics[0]->freq ||
	    arch->ics[i]->width != arch->ics[0]->width ||
	    arch->ics[i]->latency != arch->ics[0]->latency) {
	  ae_err("IC %d is not same as IC 0\n", i);
	}
      }

      break;

    default:
      ae_err("match_architecture: %d\n", ret);
    }
  }

  fflush(stdout);
}


static void parse_mappings(struct ae_mapping *map, FILE *f)
{
  int nmappings;
  int peid, taskid;
  int i;
  nmappings = ae_get_int(f);
  assert(nmappings >= 0 && nmappings <= map->ntasks);
  for (i = 0; i < nmappings; i++) {
    ae_match_word("map", f);
    taskid = ae_get_int(f);
    assert(taskid >= 0 && taskid < map->ntasks);
    peid = ae_get_int(f);
    assert(peid >= 0);
    map->mappings[taskid] = peid;
  }
}


static void match_tasks(struct ae_mapping *map, FILE *f)
{
  int i;
  int default_mapping;
  int nstatics;
  int taskid;
  char *apptype;

  apptype = ae_get_word(f);
  if (strcmp(apptype, "task_list") == 0) {
    ae_read_stg(map, f);
  } else if (strcmp(apptype, "kpn") == 0) {
    ae_read_kpn(map, f);
  } else {
    ae_err("Unknown application type: %s\n", apptype);
  }
  printf("appmodel: %s\n", map->appmodel->name);
  free(apptype);
  apptype = NULL;

  ae_match_word("default_mapping", f);
  default_mapping = ae_get_int(f);
  assert(default_mapping >= 0);
  if ((map->mappings = malloc(map->ntasks * sizeof(int))) == NULL)
    ae_err("match_tasks: no memory\n");
  if ((map->isstatic = malloc(map->ntasks * sizeof(int))) == NULL)
    ae_err("match_tasks: no memory\n");
  for (i = 0; i < map->ntasks; i++) {
    map->mappings[i] = default_mapping;
    map->isstatic[i] = 0;
  }

  ae_match_word("mapping_list", f);
  parse_mappings(map, f);

  ae_match_word("static_list", f);
  nstatics = ae_get_int(f);
  assert(nstatics >= 0 && nstatics <= map->ntasks);
  for (i = 0; i < nstatics; i++) {
    taskid = ae_get_int(f);
    assert(taskid >= 0 && taskid < map->ntasks);
    map->isstatic[taskid] = 1;
  }
}


struct ae_mapping *ae_read_input(FILE *f)
{
  char *main_categories[] = {"architecture", "tasks", "optimization", NULL};
  char *extras[] = {"mapping_list", NULL};
  int ret;
  struct ae_arch *arch;
  struct ae_mapping *map;
  int got_arch = 0;
  int got_map = 0;
  int got_opt = 0;
  int i;

  arch = ae_create_arch();
  map = ae_create_mapping();
  map->optimization = ae_create_optimization_context();
  CALLOC_ARRAY(map->appmodel, 1);

  while (got_arch == 0 || got_map == 0 || got_opt == 0) {
    ret = ae_match_alternatives(main_categories, f);
    switch (ret) {
    case 0:
      assert(got_arch == 0);
      match_architecture(arch, f);

      MALLOC_ARRAY(map->icpriorities, arch->npes);
      for (i = 0; i < arch->npes; i++)
	map->icpriorities[i] = arch->pes[i]->ic_initial_priority;

      got_arch = 1;
      break;
    case 1:
      assert(got_map == 0);
      match_tasks(map, f);
      got_map = 1;
      break;
    case 2:
      assert(got_opt == 0);
      ae_read_optimization_parameters(map->optimization, f);
      got_opt = 1;
      break;
    default:
      ae_err("very unexpected error in ae_read_input\n");
    }
  }

  map->arch = arch;

  while ((ret = ae_match_alternatives(extras, f)) != -2) {
    switch (ret) {
    case 0:
      parse_mappings(map, f);
      break;
    default:
      ae_err("unexpected extras\n");
    }
  }

  return map;
}
