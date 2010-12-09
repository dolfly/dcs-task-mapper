#ifndef _AE_ARCH_H_
#define _AE_ARCH_H_

#include "arextypes.h"

struct ae_arch *ae_create_arch(void);
void ae_energy(double *A, double *statE, double *dynE, struct ae_mapping *map);

#endif
