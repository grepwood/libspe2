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

/* This test checks if the affinity support works correctly by running
 * a simple program.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "ppu_libspe2_test.h"

#define COUNT 1000

extern spe_program_handle_t spu_arg;

static void *spe_thread_proc(void *arg)
{
  ppe_thread_t *ppe = (ppe_thread_t*)arg;
  spe_context_ptr_t *spes = (spe_context_ptr_t *)ppe->group->data;
  spe_context_ptr_t spe = spes[ppe->index];
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_stop_info_t stop_info;

  if (spe_program_load(spe, &spu_arg)) {
    eprintf("spe_program_load: %s\n", strerror(errno));
    fatal();
  }

  global_sync(NUM_SPES);

  ret = spe_context_run(spe, &entry, 0,
			(void*)RUN_ARGP_DATA, (void*)RUN_ENVP_DATA, &stop_info);
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

static int test_affinity(int mem_p, int spu_p)
{
  int ret;
  int i;

  tprintf("Memory: %s: SPU: %s\n",
	  mem_p ? "enabled" : "disabled",
	  spu_p ? "enabled" : "disabled");

  for (i = 0; i < COUNT; i++) {
    spe_gang_context_ptr_t gang;
    spe_context_ptr_t spes[NUM_SPES];
    int j;

    gang = spe_gang_context_create(0);
    if (!gang) {
      eprintf("spe_gang_context_create: %s\n", strerror(errno));
      fatal();
    }

    for (j = 0; j < NUM_SPES; j++) {
      unsigned int flags = mem_p ? (j ? 0 : SPE_AFFINITY_MEMORY) : 0;
      spe_context_ptr_t neighbor = spu_p ? (j ? spes[j - 1] : NULL) : NULL;
      spes[j] = spe_context_create_affinity(flags, neighbor, gang);
      if (!spes[j]) {
	eprintf("spe_context_create_affinity(0x%08x, %p, %p): %s\n",
		flags, neighbor, gang, strerror(errno));
	fatal();
      }
    }

    ret = ppe_thread_group_run(NUM_SPES, spe_thread_proc, spes, NULL);
    if (ret) {
      fatal();
    }

    for (j = 0; j < NUM_SPES; j++) {
      ret = spe_context_destroy(spes[j]);
      if (ret) {
	eprintf("spe_context_destroy: %s\n", strerror(errno));
	fatal();
      }
    }

    ret = spe_gang_context_destroy(gang);
    if (ret) {
      eprintf("spe_gang_context_destroy: %s\n", strerror(errno));
      fatal();
    }
  }

  return 0;
}

static int test(int argc, char **argv)
{
  int ret;

  ret = test_affinity(1, 0);
  if (ret) {
    fatal();
  }

  ret = test_affinity(0, 1);
  if (ret) {
    fatal();
  }

  ret = test_affinity(1, 1);
  if (ret) {
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
