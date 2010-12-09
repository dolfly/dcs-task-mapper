#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define _GNU_SOURCE
#include <getopt.h>

#include "input.h"
#include "optimization.h"
#include "arexbasic.h"
#include "mappingheuristics.h"
#include "result.h"
#include "sa.h"
#include "support.h"

void print_usage(void)
{
	printf(
"arex usage:\n"
"\n"
"-a p, --arb-policy=p,                Set IC arbitration policy\n"
"-f, --fast-premapping,               Apply fast pre-mapping before optim.\n"
"-h, --help,                          Print help.\n"
"-i s, --ic-priorities=s,             Set IC priorities as a string: s = 012..\n"
"                                     where 0 is the priority for the first PE,\n"
"                                     1 is the priority for the second, and so\n"
"                                     on. Example: -i 010. 1st gets 0, 2nd gets\n"
"                                     1, and 3rd gets 0.\n"
"-l, --list-mapping-heuristics,       List supported mapping heuristics.\n"
"-m name, --mapping-heuristics=name,  Select mapping heuristics.\n"
"-o file, --output=file,              Write schedule iteration statistics on\n"
"                                     memory, time, and objective values to\n"
"                                     a file. (not portable data)\n"
"-p str, --parameter=str,             Pass a parameter string for the\n"
"                                     optimization algorithm.\n"
		);
}

int main(int argc, char **argv)
{
	struct ae_mapping *map;
	int ret;
	char *sa_heur = NULL;
	struct mh_heuristics *mh = mh_heuristics;
	char *parname;
	char *parvalue;

	struct option long_options[] = {
		{"arb-policy", 1, NULL, 'a'},
		{"fast-premapping", 0, NULL, 'f'},
		{"help", 0, NULL, 'h'},
		{"heuristic", 1, NULL, 'm'},
		{"ic-priorities", 1, NULL, 'i'},
		{"list-mapping-heuristics", 0, NULL, 'l'},
		{"mapping-heuristics", 1, NULL, 'm'},
		{"parameter", 1, NULL, 'p'},
		{NULL, 0, NULL, 0}
	};

	while ((ret = getopt_long(argc, argv, "a:fhi:lm:o:p:", long_options, NULL)) != -1) {
		switch (ret) {

		case 'a':
			ae_config.arbitration_policy = xstrdup(optarg);
			break;

		case 'f':
			ae_config.fast_premapping = 1;
			break;

		case 'h':
			print_usage();
			return 0;

		case 'i':
			ae_config.ic_priorities = xstrdup(optarg);
			break;

		case 'l':
			while (mh->name != NULL) {
				printf("%s\n", mh->name);
				mh++;
			}
			return 0;

		case 'm':
			sa_heur = xstrdup(optarg);
			break;

		case 'o':
			sa_output_file = xstrdup(optarg);
			break;

		case 'p':
			if (ae_config.cmdline_optimization_parameter)
				fprintf(stderr, "Warning: losing old style optimization parameters\n");
			ae_config.cmdline_optimization_parameter = xstrdup(optarg);
			parname = xstrdup(optarg);
			parvalue = parname;
			strsep(&parvalue, "=");
			if (parvalue == NULL)
				parvalue = "";
			ae_config_append_parameter(parname, parvalue);
			break;

		case '?':
		case ':':
			ae_err("\n");

		default:
			ae_err("unknown option\n");
		}
	}

	if (optind < argc)
		ae_err("unknown argument: %s\n", argv[optind]);

	map = ae_read_input(stdin);

	if (sa_heur) {
		if (strstr(map->optimization->params, "simulated_annealing")) {
			fprintf(stderr, "warning: simulated annealing is not used, but a heuristics name was given\n");
		} else {
			ae_sa_set_heuristics(map->optimization->params, sa_heur);
			fprintf(stderr, "sa_heuristics: %s (override)\n", sa_heur);
		}
		free(sa_heur);
		sa_heur = NULL;
	}

	ae_optimize(map);
	return 0;
}
