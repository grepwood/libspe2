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

/* This test checks if the SPE_EVENT_IN_MBOX event support works
   correctly. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define COUNT 10000

extern spe_program_handle_t spu_event_wbox;

typedef struct spe_thread_params
{
  spe_context_ptr_t spe;
  int index;
  pthread_t tid;
  unsigned int num_wbox;
} spe_thread_params_t;

static void *spe_thread_proc(void *arg)
{
  spe_thread_params_t *params = (spe_thread_params_t *)arg;
  spe_context_ptr_t spe = params->spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info;
  int ret;

  if (spe_program_load(spe, &spu_event_wbox)) {
    eprintf("spe[%d]: spe_program_load: %s\n", params->index, strerror(errno));
    fatal();
  }

  global_sync(NUM_SPES);

  ret = spe_context_run(spe, &entry, 0, (void*)COUNT, 0, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      fatal();
    }
  }
  else if (ret > 0) {
    eprintf("spe[%d]: spe_context_run: Unexpected stop and signal\n", params->index);
  }
  else {
    eprintf("spe[%d]: spe_context_run: %s\n", params->index, strerror(errno));
    fatal();
  }

  return NULL;
}

static int test(int argc, char **argv)
{
  int ret;
  spe_thread_params_t params[NUM_SPES];
  spe_event_handler_ptr_t evhandler;
#define MAX_EVENT NUM_SPES
  spe_event_unit_t event[MAX_EVENT];
  int i;
  int exit_count;
  int num_events;

  /* initialize SPE contexts */
  for (i = 0; i < NUM_SPES; i++) {
    params[i].index = i;
    params[i].num_wbox = 0;
    params[i].spe = spe_context_create(SPE_EVENTS_ENABLE, NULL);
    if (!params[i].spe) {
      eprintf("spe_context_create: %s\n", strerror(errno));
      fatal();
    }
  }

  /* register events */
  evhandler = spe_event_handler_create();
  if (!evhandler) {
    eprintf("spe_event_handler_create: %s\n", strerror(errno));
    fatal();
  }

  for (i = 0; i < NUM_SPES; i++) {
    event[0].events = SPE_EVENT_IN_MBOX;
    event[0].spe = params[i].spe;
    event[0].data.u32 = i;
    ret = spe_event_handler_register(evhandler, event);
    if (ret == -1) {
      eprintf("spe_event_handler_register: %s\n", strerror(errno));
      fatal();
    }
  }

  /* run SPE contexts */
  for (i = 0; i < NUM_SPES; i++) {
    ret = pthread_create(&params[i].tid, NULL, spe_thread_proc, params + i);
    if (ret) {
      eprintf("pthread_create: %s\n", strerror(ret));
      fatal();
    }
  }

  /* event loop */
  exit_count = 0;
  while (exit_count < NUM_SPES) {
    /* wait for the next event */
    ret = num_events = spe_event_wait(evhandler, event, MAX_EVENT, -1);
    if (ret == -1) {
      eprintf("spe_event_wait: %s\n", strerror(errno));
      fatal();
    }
    else if (ret == 0) {
      eprintf("spe_event_wait: Unexpected timeout.\n");
      fatal();
    }

    /* process events */
    for (i = 0; i < num_events; i++) {
      spe_thread_params_t *cur_params;
      tprintf("event %u/%u: spe[%u]\n", i, num_events, (unsigned int)event[i].data.u32);
      cur_params = params + event[i].data.u32;
      if (event[i].events & SPE_EVENT_IN_MBOX) {
	unsigned data[WBOX_DEPTH];
	int num_wbox =
	  cur_params->num_wbox + WBOX_DEPTH > COUNT ? COUNT - cur_params->num_wbox : WBOX_DEPTH;
	int j;
	for (j = 0; j < num_wbox; j++) {
	  data[j] = cur_params->num_wbox + j + 1;
	}
	ret = spe_in_mbox_write(event[i].spe, data, num_wbox, SPE_MBOX_ANY_NONBLOCKING);
	if (ret == -1) {
	  eprintf("spe[%u]: spe_in_mbox_write: %s\n",
		  event[i].data.u32, strerror(errno));
	  fatal();
	}
	else if (ret == 0) {
	  /* It should be possible to write data immediately, just
	     after receiving 'SPE_EVENT_IN_MBOX' events. */
	  eprintf("spe[%u]: spe_in_mbox_write: wbox event received, but no space\n",
		  event[i].data.u32);
	  fatal();
	}
	cur_params->num_wbox += ret;
	if (cur_params->num_wbox >= COUNT) { /* done */
	  exit_count++;
	  event[i].spe = cur_params->spe;
	  event[i].events = SPE_EVENT_IN_MBOX;
	  ret = spe_event_handler_deregister(evhandler, event + i);
	  if (ret == -1) {
	    eprintf("spe_event_handler_deregister: %s\n", strerror(errno));
	    fatal();
	  }
	}
      }
      else {
	eprintf("event %u/%u: spe[%u]: Unexpected event (0x%08x)\n",
		i, num_events, (unsigned int)event[i].data.u32, event[i].events);
	fatal();
      }
    }
  }

  /* cleanup */
  for (i = 0; i < NUM_SPES; i++) {
    pthread_join(params[i].tid, NULL);

    ret = spe_context_destroy(params[i].spe);
    if (ret) {
      eprintf("spe_context_destroy: %s\n", strerror(errno));
      fatal();
    }
  }

  ret = spe_event_handler_destroy(evhandler);
  if (ret) {
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
