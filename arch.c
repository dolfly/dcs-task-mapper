/*
 * DCS task mapper
 * Copyright (C) 2004-2010 Tampere University of Technology
 *
 * The program was originally written by Heikki Orsila <heikki.orsila@iki.fi>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

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
	CALLOC_ARRAY(arch, 1);
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
