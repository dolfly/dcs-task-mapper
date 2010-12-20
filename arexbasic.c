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
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <limits.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "arexbasic.h"


#define NRANDS 4096


static int randomindex = 0;
static int randominvalid = 1;
static int8_t randomdata[NRANDS];
static int randominitialized;


void ae_add_resource(int *n, int *nallocated, void ***resources, void *resource)
{
  assert(*n >= 0);
  assert(*nallocated >= 0);
  assert(*n <= *nallocated);

  if (*n == *nallocated) {
    if (*nallocated == 0)
      *nallocated = 1;
    *nallocated *= 2;
    assert(*nallocated > 0);
    *resources = realloc(*resources, *nallocated * sizeof(void *));
    if (resources == NULL)
      ae_err("no memory for resource\n");
  }

  (*resources)[*n] = resource;
  (*n)++;
}


static void ae_fill_buffer(void)
{
  int bytes;

  if (!randominitialized) {
    struct timeval tv;

    assert(RAND_MAX >= 65535);

    if (!gettimeofday(&tv, NULL)) {
      unsigned int seed = ((unsigned int) tv.tv_sec) ^ ((unsigned int) tv.tv_usec);
      srandom(seed);
    }

    randominitialized = 1;
  }

  for (bytes = 0; bytes < NRANDS; bytes += 2) {
    * (uint16_t *) &randomdata[bytes] = (uint16_t) (65536.0 * (random() / (RAND_MAX + 1.0)));
  }
}


void *ae_fork_memory(void *src, size_t size)
{
  void *dst = malloc(size);
  if (dst == NULL)
    ae_err("could not fork memory\n");
  memcpy(dst, src, size);
  return dst;
}


static void get_random(void *buf, int nbytes)
{
  assert(nbytes > 0 && nbytes <= NRANDS);
  assert(randomindex >= 0 && randomindex <= NRANDS);

  if (randominvalid != 0 || (NRANDS - randomindex) < nbytes) {
    ae_fill_buffer();
    randominvalid = 0;
    randomindex = 0;
  }

  memcpy(buf, &randomdata[randomindex], nbytes);

  randomindex += nbytes;
}


/* Returns a double in range [a, b) */
double ae_randd(double a, double b)
{
  uint64_t x;
  double res;

  assert(a < b);

  get_random(&x, sizeof(x));

  x &= (1LL << 56) - 1;
  res = a + (b - a) * ((double) x) / ((double) (1LL << 56));

  return res;
}


/* Returns an integer x that satisfies a <= x < b */
int ae_randi(int a, int b)
{
  unsigned int x;
  int res;

  assert(a < b);
  assert(sizeof(int) == 4);

  get_random(&x, sizeof(x));

  res = a + ((uint64_t) x) * (b - a) / (((uint64_t) UINT_MAX) + 1);

  return res;
}


void ae_random_cards(int *cards, int n, int maximum)
{
  int cardid;
  int i;
  int *lottery;
  int electeesleft, randi;

  assert(maximum > 0);
  assert(n > 0);
  assert(n <= maximum);

  if ((lottery = malloc(sizeof(lottery[0]) * maximum)) == NULL)
    ae_err("%s: not enough memory for lottery\n", __func__);

  for (cardid = 0; cardid < maximum; cardid++)
    lottery[cardid] = cardid;

  electeesleft = maximum;

  for (i = 0; i < n; i++) {
    randi = ae_randi(0, electeesleft);
    cards[i] = lottery[randi];
    lottery[randi] = lottery[electeesleft - 1];
    electeesleft--;
  }

  free(lottery);
}
