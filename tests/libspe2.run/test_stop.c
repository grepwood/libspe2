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

/* This test checks if the library can handle the result of stop
 * instruction correctly for each stop code.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define STOP_AND_SIGNAL_MIN 1
#define STOP_AND_SIGNAL_MAX 0x3ffe

extern spe_program_handle_t spu_stop;

static void *spe_thread_proc(void *arg)
{
  int ret;
  int i;
  spe_context_ptr_t spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;

  spe = spe_context_create(0, NULL);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    fatal();
  }

  if (spe_program_load(spe, &spu_stop)) {
    eprintf("spe_program_load(%p, &spu_stop): %s\n", spe, strerror(errno));
    fatal();
  }

  /* synchronize all threads */
  global_sync(NUM_SPES);

  for (i = STOP_AND_SIGNAL_MIN; i <= STOP_AND_SIGNAL_MAX; i++) {
    spe_stop_info_t stop_info;
    spe_stop_info_t expected;

    if (i == 0x2206) {
      /* FIXME: this value should be treated in more 'generic' way. */

      /* won't be reported. skip test */
      continue;
    }

    ret = spe_context_run(spe, &entry, SPE_NO_CALLBACKS,
			  (void*)STOP_AND_SIGNAL_MIN,
			  (void*)(STOP_AND_SIGNAL_MAX + 1),
			  &stop_info);
    if (i == 0) {
      /* FIXME: data executed must be reported as error. */
    }
    else if (i <= 0x1fff) {
    user_signal:
      expected.stop_reason = SPE_STOP_AND_SIGNAL;
      expected.result.spe_signal_code = i;
      if (check_stop_info(&stop_info, &expected)) {
	failed();
      }
      if (ret != i) {
	eprintf("Unexpected return code: 0x%04x\n", ret);
	failed();
      }
    }
    else if (i <= 0x20ff) {
      expected.stop_reason = SPE_EXIT;
      expected.result.spe_signal_code = (i & 0xff);
      if (check_stop_info(&stop_info, &expected)) {
	failed();
      }
      if (ret != 0) {
	eprintf("Unexpected return code: 0x%04x\n", ret);
	failed();
      }
    }
    else if (i <= 0x21ff) {
      goto user_signal;
    }
    else if (i <= 0x220f) {
      expected.stop_reason = SPE_ISOLATION_ERROR;
      expected.result.spe_isolation_error = (i & 0xf);
      if (check_stop_info(&stop_info, &expected)) {
	failed();
      }
      if (ret != -1) {
	eprintf("Unexpected return code: 0x%04x\n", ret);
	failed();
      }
    }
    else {
      goto user_signal;
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
