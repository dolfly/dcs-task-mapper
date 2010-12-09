#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "mappingheuristics.h"
#include "mapping.h"
#include "arexbasic.h"
#include "task.h"


static void mh_new_chain(struct ae_mapping *m, struct ae_mapping *m_old, double T, int single)
{
  double u;
  int max_deepness;
  int peid;
  int *assigned;
  int nassigned;
  struct ae_lifo_entry {
    int taskid;
    int deepness;
  };
  struct ae_lifo_entry *lifo;
  int lifon;
  int taskid, deepness;
  int parentid, temp;
  struct ae_task *task;

  ae_copy_mapping(m, m_old);
  if (m->arch->npes == 1)
    return;

  u = ae_randd(0, 1);
  if (u == 0.0)
    u = 0.5;
  max_deepness = -floor(log(u)/log(2));

  peid = ae_randi(0, m->arch->npes);

  CALLOC_ARRAY(assigned, m->ntasks);
  nassigned = 0;

  MALLOC_ARRAY(lifo, m->ntasks);
  lifon = 1;

  lifo[0].taskid = ae_randi(0, m->ntasks);
  lifo[0].deepness = 1;

  while (lifon > 0) {
    lifon--;
    taskid = lifo[lifon].taskid;
    assert(taskid >= 0 && taskid < m->ntasks);
    deepness = lifo[lifon].deepness;
    nassigned++;
    ae_set_mapping(m, taskid, peid);
    if (deepness >= max_deepness)
      continue;

    task = m->tasks[taskid];

    if (single) {
      if (task->nin > 0) {
	parentid = task->in[ae_randi(0, task->nin)];
	if (assigned[parentid])
	  continue;
	assigned[parentid] = 1;
	lifo[lifon].taskid = parentid;
	lifo[lifon].deepness = deepness + 1;
	lifon++;	
      }
    } else {
      AE_FOR_EACH_PARENT(task, parentid, temp) {
	if (assigned[parentid])
	  continue;
	assigned[parentid] = 1;
	lifo[lifon].taskid = parentid;
	lifo[lifon].deepness = deepness + 1;
	lifon++;
      }
    }
  }
  /* fprintf(stderr, "nassigned: %d\n", nassigned); */
}


static void mh_chain_setting_single(struct ae_mapping *m,
				    struct ae_mapping *m_old, double T,
				    struct optstate *os)
{
	mh_new_chain(m, m_old, T, 1);
}


static void mh_chain_setting_multiple(struct ae_mapping *m,
				      struct ae_mapping *m_old, double T,
				      struct optstate *os)
{
	mh_new_chain(m, m_old, T, 0);
}

static void mh_rm(struct ae_mapping *m, struct ae_mapping *m_old, double T,
		  struct optstate *os)
{
	T = T;
	ae_copy_mapping(m, m_old);
	ae_randomize_n_task_mappings(m, 1);
}

static void mh_rm_adaptive(struct ae_mapping *m, struct ae_mapping *m_old,
			   double T, struct optstate *os)
{
	int n = 1;
	struct optmoveprobabilities ps;

	T = T;

	if (opt_move_probabilities(&ps, os)) {
		int c1 = (ps.psame == 0 && ps.pbetter < 0.5);
		int c2 = (ps.pworse >= 0.75);
		int c3 = (ps.psame >= 0.25);
		int c4 = (ps.pworse <= 0.25);
		if (!c1 && !c2 && (c3 || c4))
			n = 2;
		/* printf("move probabilities: %.3f %.3f %.3f %d %d %d %d : n = %d\n", ps.pworse, ps.psame, ps.pbetter, c1, c2, c3, c4, n); */
	}

	ae_copy_mapping(m, m_old);
	ae_randomize_n_task_mappings(m, n);
}

static void mh_rmdt(struct ae_mapping *m, struct ae_mapping *m_old, double T,
		    struct optstate *os)
{
	int n;
	ae_copy_mapping(m, m_old);
	n = floor(T * m->ntasks);
	if (n == 0)
		n = 1;
	ae_randomize_n_task_mappings(m, n);
}


struct mh_heuristics mh_heuristics[] = {
	{"csm", mh_chain_setting_multiple},
	{"css", mh_chain_setting_single},
	{"rm", mh_rm},
	{"rmdt", mh_rmdt},
	{"rm-adaptive", mh_rm_adaptive},
	{NULL, NULL}
};
