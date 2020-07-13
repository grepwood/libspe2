/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2008 Sony Computer Entertainment Inc.
 *  Copyright 2007,2008 Sony Corp.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/* This test checks if the library can handle affinity-related errors
 * correctly.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "ppu_libspe2_test.h"

extern spe_program_handle_t spu_null;

static void *spe_thread_proc(void *arg)
{
  ppe_thread_t *ppe = (ppe_thread_t*)arg;
  spe_context_ptr_t *spes = (spe_context_ptr_t *)ppe->group->data;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_stop_info_t stop_info;

  if (spe_program_load(spes[ppe->index], &spu_null)) {
    eprintf("spe_program_load: %s\n", strerror(errno));
    fatal();
  }

  ret = spe_context_run(spes[ppe->index], &entry, 0, NULL, NULL, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      fatal();
    }
  }
  else {
    eprintf("spe_context_run: %s\n", strerror(errno));
    fatal();
  }

  return NULL;
}

static int test(int argc, char **argv)
{
  spe_context_ptr_t spes[NUM_SPES];
  spe_gang_context_ptr_t gang;
  spe_gang_context_ptr_t gang2;
  int ret;
  int i;
  int num_spes;
  int max_spes_per_be;

  gang = spe_gang_context_create(0);
  if (!gang) {
    eprintf("spe_gang_context_create: %s\n", strerror(errno));
    fatal();
  }

  num_spes = 0;

  spes[num_spes] = spe_context_create_affinity(SPE_AFFINITY_MEMORY, NULL, gang);
  if (!spes[num_spes]) {
    eprintf("spe_context_create_affinity(SPE_AFFINITY_MEMORY, NULL, %p): %s\n",
	    gang, strerror(errno));
    fatal();
  }
  num_spes++;

  /* check # of SPEs */
  max_spes_per_be = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, 0);
  if (max_spes_per_be < 0) {
    eprintf("spe_cpu_info_get(SPE_COUNT_USABLE_SPES, 0): %s\n", strerror(errno));
    fatal();
  }

  /* check EEXIST cases */
  spes[num_spes] = spe_context_create_affinity(SPE_AFFINITY_MEMORY, NULL, gang);
  if (spes[num_spes]) {
    eprintf("spe_context_create_affinity(SPE_AFFINITY_MEMORY, NULL, %p): unexpected success.\n",
	    gang);
    fatal();
  }
  else if (errno != EEXIST) {
    eprintf("spe_context_create_affinity(SPE_AFFINITY_MEMORY, NULL, %p): unexpected errno: %d (%s)\n",
	    gang, errno, strerror(errno));
    failed();
  }

  spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 1], gang);
  if (!spes[num_spes]) {
    eprintf("spe_context_create_affinity(0, %p, %p): %s.\n",
	    spes[num_spes - 1], gang, strerror(errno));
    fatal();
  }
  num_spes++;
  spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 1], gang);
  if (!spes[num_spes]) {
    eprintf("spe_context_create_affinity(0, %p, %p): %s.\n",
	    spes[num_spes - 1], gang, strerror(errno));
    fatal();
  }
  num_spes++;
  spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 2], gang);
  if (spes[num_spes]) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected success.\n",
	    spes[num_spes - 2], gang);
    fatal();
  }
  else if (errno != EEXIST) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected errno: %d (%s)\n",
	    spes[num_spes - 2], gang, errno, strerror(errno));
    failed();
  }

  /* check ESRCH cases */
  gang2 = spe_gang_context_create(0);
  if (!gang2) {
    eprintf("spe_gang_context_create(0): %s\n", strerror(errno));
    fatal();
  }
  spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 1], gang2);
  if (spes[num_spes]) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected success.\n",
	    spes[num_spes - 1], gang2);
    fatal();
  }
  ret = spe_gang_context_destroy(gang2);
  if (ret) {
    eprintf("spe_gang_context_destroy(%p): %s\n", gang2, strerror(errno));
    fatal();
  }

  /* check EEXIST cases (cont.) */
  for (i = num_spes; i < max_spes_per_be; i++) {
    spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 1], gang);
    if (!spes[num_spes]) {
      eprintf("spe_context_create_affinity(0, %p, %p): %s.\n",
	      spes[num_spes - 1], gang, strerror(errno));
      fatal();
    }
    num_spes++;
  }
  spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 1], gang);
  if (spes[num_spes]) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected success.\n",
	    spes[num_spes - 1], gang);
    fatal();
  }
  else if (errno != EEXIST) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected errno: %d (%s)\n",
	    spes[num_spes - 1], gang, errno, strerror(errno));
    failed();
  }

  /* check EBUSY cases */
  --num_spes;
  ret = spe_context_destroy(spes[num_spes]); /* remove a context from the gang. */
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spes[num_spes], strerror(errno));
    fatal();
  }
  /* run */
  ret = ppe_thread_group_run(num_spes, spe_thread_proc, spes, NULL);
  if (ret) {
    fatal();
  }
  /* try to add more context to the gang. */
  spes[num_spes] = spe_context_create_affinity(0, spes[num_spes - 1], gang);
  if (spes[num_spes]) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected success.\n",
	    spes[num_spes - 1], gang);
    fatal();
  }
  else if (errno != EBUSY) {
    eprintf("spe_context_create_affinity(0, %p, %p): unexpected errno: %d (%s)\n",
	    spes[num_spes - 1], gang, errno, strerror(errno));
    failed();
  }

  for (i = 0; i < num_spes; i++) {
    ret = spe_context_destroy(spes[i]);
    if (ret) {
      eprintf("spe_context_destroy(%p): %s\n", spes[i], strerror(errno));
      fatal();
    }
  }

  ret = spe_gang_context_destroy(gang);
  if (ret) {
    eprintf("spe_gang_context_destroy(%p): %s\n", gang, strerror(errno));
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
