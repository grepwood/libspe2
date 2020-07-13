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

/* This test checks if the all mailboxes support work correctly at the
 * same time.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <unistd.h>
#include <sys/time.h>

#include "ppu_libspe2_test.h"

#define COUNT 10000

typedef struct test_params
{
  const char *name;
  unsigned int flags;
} test_params_t;

typedef struct spe_thread_params
{
  spe_context_ptr_t spe;
  ppe_thread_t *ppe;
} spe_thread_params_t;

extern spe_program_handle_t spu_mbox_all;

static void *spe_thread_proc(void *arg)
{
  spe_thread_params_t *params = (spe_thread_params_t *)arg;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_stop_info_t stop_info;

  if (spe_program_load(params->spe, &spu_mbox_all)) {
    eprintf("spe_program_load(%p, &spu_mbox_all): %s\n", params->spe, strerror(errno));
    fatal();
  }

  /* synchronize all threads */
  global_sync(NUM_SPES * 2);

  ret = spe_context_run(params->spe, &entry, 0, (void*)COUNT, NULL, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      fatal();
    }
  }
  else {
    eprintf("spe_context_run(%p, ...): %s\n", params->spe, strerror(errno));
    fatal();
  }

  return NULL;
}

static void *ppe_thread_proc(void *arg)
{
  ppe_thread_t *ppe = (ppe_thread_t*)arg;
  spe_thread_params_t spe_params;
  pthread_t tid;
  test_params_t *params = (test_params_t *)ppe->group->data;
  int ret;
  int i;
  unsigned int data_buf[WBOX_DEPTH];

  spe_params.ppe = ppe;
  spe_params.spe = spe_context_create(params->flags, 0);
  if (!spe_params.spe) {
    eprintf("%s: spe_context_create(0x%08x, NULL): %s\n",
	    params->name, params->flags, strerror(errno));
    fatal();
  }

  ret = pthread_create(&tid, NULL, spe_thread_proc, &spe_params);
  if (ret) {
    eprintf("%s: pthread_create: %s\n", params->name, strerror(ret));
    fatal();
  }

  /* synchronize all threads */
  global_sync(NUM_SPES * 2);

  for (i = 0; i < COUNT; i++) {
    unsigned int test_data = rand32();
    unsigned int behavior;
    unsigned int retry_count;
    unsigned int done;
    int j;

    /*** inbound mailbox tests ***/

    /* initialize data */
    for (j = 0; j < WBOX_DEPTH; j++) {
      data_buf[j] = test_data + j;
    }

    /* choose behavior */
    switch (i % 3) {
    case 0:
      behavior = SPE_MBOX_ALL_BLOCKING;
      break;
    case 1:
      behavior = SPE_MBOX_ANY_BLOCKING;
      break;
    default:
      behavior = SPE_MBOX_ANY_NONBLOCKING;
      break;
    }

    /* write to inbound mailbox */
    retry_count = 0;
    done = 0;
    while (done < WBOX_DEPTH) {
      ret = spe_in_mbox_write(spe_params.spe, data_buf + done, WBOX_DEPTH - done, behavior);
      if (ret == -1) { /* error */
	eprintf("%s: spe_in_mbox_write: %s\n", params->name, strerror(errno));
	fatal();
      }
      switch (behavior) {
      case SPE_MBOX_ALL_BLOCKING:
	if (ret != WBOX_DEPTH) { /* all data must be written */
	  eprintf("%s: spe_in_mbox_write(%s) not completed.\n",
		  params->name, mbox_behavior_symbol(behavior));
	  fatal();
	}
	break;
      case SPE_MBOX_ANY_BLOCKING:
	if (ret == 0) { /* any data must be written */
	  eprintf("%s: spe_in_mbox_write(%s): no data was written.\n",
		  params->name, mbox_behavior_symbol(behavior));
	  fatal();
	}
	else if (ret != WBOX_DEPTH - done) { /* any data is remained */
	  tprintf("spe_in_mbox_write: Written %d of %d \n", ret, WBOX_DEPTH - done);
	}
	break;
      default:
	if (ret == 0) { /* no data was written */
	  retry_count++;
	}
	break;
      }
      done += ret;
      if (done > WBOX_DEPTH) {
	eprintf("%s: spe_in_mbox_write(%s) too many data.\n",
		params->name, mbox_behavior_symbol(behavior));
	fatal();
      }
    }

    if (retry_count) {
      tprintf("spe_in_mbox_write: Retry %d times.\n", retry_count);
    }

    /*** outbound interrupt mailbox tests ***/

    /* choose behavior */
    switch ((i >> 2) % 3) {
    case 1:
      behavior = SPE_MBOX_ALL_BLOCKING;
      break;
    case 2:
      behavior = SPE_MBOX_ANY_BLOCKING;
      break;
    default:
      behavior = SPE_MBOX_ANY_NONBLOCKING;
      break;
    }

    /* read from outbound interrupt mailbox */
    retry_count = 0;
    done = 0;
    while (done < IBOX_DEPTH) {
      ret = spe_out_intr_mbox_read(spe_params.spe, data_buf + done, IBOX_DEPTH - done, behavior);
      if (ret == -1) {
	eprintf("%s: spe_out_intr_mbox_read: %s\n", params->name, strerror(errno));
	fatal();
      }
      switch (behavior) {
      case SPE_MBOX_ALL_BLOCKING:
	if (ret != IBOX_DEPTH) { /* all data must be read */
	  eprintf("%s: spe_out_intr_mbox_read(%s) not completed.\n",
		  params->name, mbox_behavior_symbol(behavior));
	  fatal();
	}
	break;
      case SPE_MBOX_ANY_BLOCKING:
	if (ret == 0) { /* any data must be read */
	  eprintf("%s: spe_out_intr_mbox_read(%s): no data was read.\n",
		  params->name, mbox_behavior_symbol(behavior));
	  fatal();
	}
	else if (ret != IBOX_DEPTH - done) { /* any data is remained */
	  tprintf("spe_out_intr_mbox_read: Read %d of %d \n", ret, IBOX_DEPTH - done);
	}
	break;
      default:
	if (ret == 0) { /* no data was read */
	  retry_count++;
	}
	break;
      }
      done += ret;
      if (done > IBOX_DEPTH) {
	eprintf("%s: spe_out_intr_mbox_read(%s) too many data.\n",
		params->name, mbox_behavior_symbol(behavior));
	fatal();
      }
    }

    if (retry_count) {
      tprintf("spe_out_intr_mbox_read: Retry %d times.\n", retry_count);
    }

    if (data_buf[0] != test_data + 1) {
      eprintf("%s: spe_out_intr_mbox_read(%s): data mismatch (%u <-> %u).\n",
	      params->name, mbox_behavior_symbol(behavior), test_data + 1, data_buf[0]);
      fatal();
    }

    /*** outbound mailbox tests ***/

    done = 0;
    while (done < MAX_MBOX_COUNT) {
      ret = spe_out_mbox_read(spe_params.spe, data_buf + done, MAX_MBOX_COUNT - done);
      if (ret == -1) {
	eprintf("%s: spe_out_mbox_read: %s\n", params->name, strerror(errno));
	fatal();
      }
      done += ret;
    }

    if (done > MAX_MBOX_COUNT) {
      eprintf("%s: spe_out_mbox_read: Too many data\n", params->name);
      fatal();
    }

    for (j = 0; j < MAX_MBOX_COUNT; j++) {
      if (data_buf[j] != test_data + 2 + j) {
	eprintf("%s: spe_out_mbox_read: data mismatch (%u <-> %u).\n",
		params->name, test_data + 2 + j, data_buf[j]);
	fatal();
      }
    }

    /* outbound mailbox must be empty here */
    ret = spe_out_mbox_read(spe_params.spe, data_buf, 1);
    if (ret == -1) {
      eprintf("%s: spe_out_mbox_read: %s\n", params->name, strerror(errno));
      fatal();
    }
    else if (ret) {
      eprintf("%s: spe_out_mbox_read: unexpected data. mbox should be empty.\n", params->name);
      fatal();
    }
  }

  pthread_join(tid, NULL);
  ret = spe_context_destroy(spe_params.spe);
  if (ret) {
    eprintf("%s: spe_context_destroy: %s\n", params->name, strerror(errno));
    fatal();
  }

  return NULL;
}

static int test(int argc, char **argv)
{
  int ret;
  test_params_t params;

  params.name = "default";
  params.flags = 0;
  ret = ppe_thread_group_run(NUM_SPES, ppe_thread_proc, &params, NULL);
  if (ret) {
    fatal();
  }

  params.name = "SPE_MAP_PS";
  params.flags = SPE_MAP_PS;
  ret = ppe_thread_group_run(NUM_SPES, ppe_thread_proc, &params, NULL);
  if (ret) {
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
