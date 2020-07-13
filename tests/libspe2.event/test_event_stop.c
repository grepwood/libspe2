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

/* This test checks if the SPE_EVENT_SPE_STOPPED event support works
   correctly. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define COUNT 3000

extern spe_program_handle_t spu_event_stop;

typedef struct spe_thread_params
{
  spe_context_ptr_t spe;
  int index;
  pthread_t tid;
  unsigned int num_stop;
} spe_thread_params_t;

static const spe_stop_info_t expected_stop_info = {
  .stop_reason = SPE_STOP_AND_SIGNAL,
  .result.spe_signal_code = STOP_DATA,
};

static void *spe_thread_proc(void *arg)
{
  spe_thread_params_t *params = (spe_thread_params_t *)arg;
  spe_context_ptr_t spe = params->spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info;
  int ret;
  unsigned int i;

  if (spe_program_load(spe, &spu_event_stop)) {
    eprintf("spe[%d]: spe_program_load: %s\n", params->index, strerror(errno));
    fatal();
  }

  global_sync(NUM_SPES);

  for (i = 0; ; i++) { /* run until the SPE exits */
    ret = spe_context_run(spe, &entry, 0,
			  (void*)STOP_DATA, (void*)COUNT, &stop_info);
    if (ret == 0) { /* exit */
      if (i != COUNT) {
	eprintf("spe[%d]: spe_context_run: Unexpected exit\n", params->index);
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
      eprintf("spe[%d]: spe_context_run: %s\n", params->index, strerror(errno));
      fatal();
    }
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
  spe_stop_info_t stop_info;
  int num_events;

  /* initialize SPE contexts */
  for (i = 0; i < NUM_SPES; i++) {
    params[i].index = i;
    params[i].num_stop = 0;
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
    event[0].events = SPE_EVENT_SPE_STOPPED;
    event[0].spe = params[i].spe;
    event[0].data.u32 = i;
    ret = spe_event_handler_register(evhandler, event);
    if (ret == -1) {
      eprintf("spe_event_handler_register: %s\n", strerror(errno));
      fatal();
    }
  }

  /* should be empty */
  ret = spe_stop_info_read(params[0].spe, &stop_info);
  if (ret == 0) {
    eprintf("spe_stop_info_read: unexpected data.\n");
    fatal();
  }
  else if (ret == -1) {
    if (errno != EAGAIN) {
      eprintf("spe_stop_info_read: %s\n", strerror(errno));
      fatal();
    }
  }
  else {
    eprintf("spe_stop_info_read: unexpected return code (%d).\n", ret);
    fatal();
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
      if (event[i].events & SPE_EVENT_SPE_STOPPED) {
	ret = spe_stop_info_read(event[i].spe, &stop_info);
	if (ret == -1) {
	  eprintf("spe[%u]: spe_stop_info_read: %s\n",
		  event[i].data.u32, strerror(errno));
	  fatal();
	}
	else if (stop_info.stop_reason == SPE_EXIT) {
	  if (check_exit_code(&stop_info, 0)) {
	    fatal();
	  }
	  exit_count++;
	}
	else { /* user stop code */
	  if (check_stop_info(&stop_info, &expected_stop_info)) {
	    fatal();
	  }
	  cur_params->num_stop++;
	}
      }
      else {
	eprintf("event %u/%u: spe[%u]: Unexpected event (0x%08x)\n",
		i, num_events, (unsigned int)event->data.u32, event->events);
	fatal();
      }
    }
  }

  /* cleanup and check result */
  for (i = 0; i < NUM_SPES; i++) {
    pthread_join(params[i].tid, NULL);

    if (params[i].num_stop > COUNT) {
      eprintf("spe[%u]: too many events (%u/%u).\n", i, params[i].num_stop, COUNT);
      failed();
    }
    else if (params[i].num_stop < COUNT) {
      eprintf("spe[%u]: events were lost (%u/%u).\n", i, params[i].num_stop, COUNT);
      failed();
    }

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
