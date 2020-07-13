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

/* This test checks if the inbound mailbox support works correctly
 * when an SPE context is accessed by multiple threads at the same
 * time.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#include <unistd.h>

#include "ppu_libspe2_test.h"

#define COUNT 10000
#define NUM_THREADS 100
#define COUNT_TOTAL (COUNT * NUM_THREADS)

extern spe_program_handle_t spu_wbox;

static void *ppe_thread_proc(void *arg)
{
  ppe_thread_t *ppe = (ppe_thread_t *)arg;
  spe_context_ptr_t spe = (spe_context_ptr_t)ppe->group->data;
  int ret;
  unsigned int wbox_value;
  unsigned int data[WBOX_DEPTH + 1];
  int i;

  /* synchronize all threads */
  global_sync(NUM_THREADS + 1);

  for (wbox_value = 0, i = 0; wbox_value < COUNT; i++) {
    unsigned int behavior;
    int num_data;
    int j;

    /* choose behavior */
    switch (i % 3) {
    case 0:
      behavior = SPE_MBOX_ANY_BLOCKING;
      break;
    case 1:
      behavior = SPE_MBOX_ALL_BLOCKING;
      break;
    default:
      behavior = SPE_MBOX_ANY_NONBLOCKING;
      break;
    }

    /* determine how many data to be read at once */
    num_data = i % (WBOX_DEPTH + 1) + 1; /* 1 ... WBOX_DEPTH + 1 */
    if (num_data > COUNT - wbox_value)
      num_data = COUNT - wbox_value;

    /* fill data */
    for (j = 0; j < num_data; j++) {
      data[j] = wbox_value + 1 + j + (COUNT * ppe->index /* offset*/);
    }

    ret = spe_in_mbox_write(spe, data, num_data, behavior);
    if (ret == -1) {
      eprintf("spe_in_mbox_write(%p, %p, %u, %s): %s\n",
	      spe, data, num_data, mbox_behavior_symbol(behavior),
	      strerror(errno));
      fatal();
    }
    if (ret > num_data) {
      eprintf("wbox: Too many (%u -> %u) wbox data.\n", num_data, ret);
      fatal();
    }
    switch (behavior) {
    case SPE_MBOX_ANY_BLOCKING:
      if (ret == 0) {
	eprintf("wbox: No wbox data.\n");
	fatal();
      }
      break;
    case SPE_MBOX_ALL_BLOCKING:
      if (ret != num_data) {
	eprintf("wbox: Too few (%u -> %u) wbox data.\n", num_data, ret);
	fatal();
      }
      break;
    default:
      break;
    }

    wbox_value += ret;
  }

  return NULL;
}

static int test(int argc, char **argv)
{
  spe_context_ptr_t spe;
  ppe_thread_group_t *ppe_group;
  int ret;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info;

  spe = spe_context_create(0, 0);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    fatal();
  }

  if (spe_program_load(spe, &spu_wbox)) {
    eprintf("spe_program_load(%p, &spu_wbox): %s\n", spe, strerror(errno));
    fatal();
  }

  ppe_group = ppe_thread_group_create(NUM_THREADS, ppe_thread_proc, spe);
  if (!ppe_group) {
    fatal();
  }

  /* synchronize all threads */
  global_sync(NUM_THREADS + 1);

  ret = spe_context_run(spe, &entry, 0, (void*)COUNT_TOTAL, NULL, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      fatal();
    }
  }
  else {
    eprintf("spe_context_run(%p, ...): %s\n", spe, strerror(errno));
    fatal();
  }

  ppe_thread_group_wait(ppe_group, NULL);

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
