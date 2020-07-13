/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2008 Sony Computer Entertainment Inc.
 *  Copyright 2008 Sony Corp.
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

/* This test checks if multiple SPE contexts can be created and be run
 * at the same time correctly.
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "ppu_libspe2_test.h"

extern spe_program_handle_t spu_arg;

static int num_spes;

static void *spe_thread_proc(void *arg)
{
  spe_context_ptr_t spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_stop_info_t stop_info;

  /* synchronize all threads to check whether the library is
   * thread-safe.
   */
  global_sync(num_spes);

  spe = spe_context_create(SPE_NOSCHED, 0);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    fatal();
  }

  if (spe_program_load(spe, &spu_arg)) {
    eprintf("spe_program_load(%p, &spu_arg): %s\n", spe, strerror(errno));
    fatal();
  }

  ret = spe_context_run(spe, &entry, 0,
			(void*)RUN_ARGP_DATA, (void*)RUN_ENVP_DATA, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      fatal();
    }
  }
  else {
    eprintf("spe_context_run(%p, ...): %s\n", spe, strerror(errno));
    fatal();
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    fatal();
  }

  return NULL;
}

static int test(int argc, char **argv)
{
  int ret;

  num_spes = spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, -1);
  if (num_spes <= 0) {
    eprintf("Could not get # of physical SPEs\n");
    fatal();
  }

  /* this test case works only as root */
  if (getuid() == 0) {
    ret = ppe_thread_group_run(num_spes, spe_thread_proc, NULL, NULL);
    if (ret) {
      fatal();
    }
  }
  else {
    eprintf("Root privilege is required. Ignored.\n");
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
