#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "arch.h"
#include "arexbasic.h"
#include "arextypes.h"
#include "schedule.h"
#include "optimization.h"

struct ae_arch *ae_create_arch(void)
{
  struct ae_arch *arch;
  if ((arch = calloc(1, sizeof(*arch))) == NULL)
    ae_err("no memory for new arch\n");
  return arch;
}

void ae_energy(double *A, double *statE, double *dynE,
	       struct ae_mapping *map)
{
  int i;
  double Atot = 0.0;
  double fmax = 0;
  struct ae_arch *arch = map->arch;
  double dynP = 0.0;
  double T = map->schedule->schedule_length;
  double k = map->optimization->power_k;

  assert(T > 0.0);
  assert(k >= 0.0);

  for (i = 0; i < arch->npes; i++) {
    struct ae_pe *pe = arch->pes[i];

    Atot += pe->area;
    fmax = MAX(fmax, pe->freq);

    dynP += pe->area * pe->freq * map->schedule->pe_utilisations[i];
  }

  for (i = 0; i < arch->nics; i++) {
    struct ae_interconnect *ic = arch->ics[i];

    Atot += ic->area;
    fmax = MAX(fmax, ic->freq);

    dynP += ic->area * ic->freq * map->schedule->ic_utilisations[i];
  }

  if (A != NULL)
    *A = Atot;

  if (statE != NULL)
    *statE = T * Atot * fmax;

  if (dynE != NULL)
    *dynE =  T * k * dynP;
}
