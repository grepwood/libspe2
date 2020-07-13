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

/* This test checks that the libspe2 doesn't stall even if no stop
 * event handler is registered.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define COUNT 50000

extern spe_program_handle_t spu_event_stop;

static const spe_stop_info_t expected_stop_info = {
  .stop_reason = SPE_STOP_AND_SIGNAL,
  .result.spe_signal_code = STOP_DATA,
};

static void *spe_thread_proc(void *arg)
{
  ppe_thread_t *ppe = (ppe_thread_t*)arg;
  spe_context_ptr_t spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info, stop_info2;
  int ret;
  unsigned int i;

  spe = spe_context_create(SPE_EVENTS_ENABLE, NULL);
  if (!spe) {
    eprintf("spe_context_create: %s\n", strerror(errno));
    fatal();
  }

  if (spe_program_load(spe, &spu_event_stop)) {
    eprintf("spe[%d]: spe_program_load: %s\n", ppe->index, strerror(errno));
    fatal();
  }

  global_sync(NUM_SPES);

  for (i = 0; ; i++) { /* run until the SPE exits */
    ret = spe_context_run(spe, &entry, 0,
			  (void*)STOP_DATA, (void*)COUNT, &stop_info);
    if (ret == 0) { /* exit */
      if (i != COUNT) {
	eprintf("spe[%d]: spe_context_run: Unexpected exit\n", ppe->index);
	fatal();
      }
      if (check_exit_code(&stop_info, 0)) {
	fatal();
      }
      break;
    }
    else if (ret > 0) { /* user stop code */
      if (check_stop_info(&stop_info, &expected_stop_info)) {
	fatal();
      }
    }
    else { /* error */
      eprintf("spe[%d]: spe_context_run: %s\n", ppe->index, strerror(errno));
      fatal();
    }

    ret = spe_stop_info_read(spe, &stop_info2);
    if (ret != 0) {
      eprintf("spe[%d]: spe_stop_info_read: Unexpected return code\n", ppe->index, ret);
      fatal();
    }
    if (check_stop_info(&stop_info2, &expected_stop_info)) {
      fatal();
    }
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

  ret = ppe_thread_group_run(NUM_SPES, spe_thread_proc, NULL, NULL);
  if (ret) {
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
