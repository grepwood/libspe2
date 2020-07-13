/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2007 Sony Computer Entertainment Inc.
 *  Copyright 2007 Sony Corp.
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

/* This test checks if the event support works correctly. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "ppu_libspe2_test.h"

#define IBOX_COUNT 3000
#define DMA_SIZE MAX_DMA_SIZE

extern spe_program_handle_t spu_event;

typedef struct spe_thread_params
{
  spe_context_ptr_t spe;
  int index;
  pthread_t tid;
  barrier_t restart;
  unsigned int num_ibox;
  unsigned int dma_ls;
  int dma_tag;
  int queued_dma;
} spe_thread_params_t;

static void *spe_thread_proc(void *arg)
{
  spe_thread_params_t *params = (spe_thread_params_t *)arg;
  spe_context_ptr_t spe = params->spe;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  spe_stop_info_t stop_info;
  int ret;

  if (spe_program_load(spe, &spu_event)) {
    eprintf("spe[%d]: spe_program_load: %s\n", params->index, strerror(errno));
    fatal();
  }

  for ( ; ; ) {
    ret = spe_context_run(spe, &entry, 0,
			  (void*)IBOX_COUNT,
			  (void*)(uintptr_t)get_timebase_frequency(), &stop_info);
    if (ret == 0) {
      if (check_exit_code(&stop_info, EXIT_DATA)) {
	fatal();
      }
      break;
    }
#ifdef ENABLE_TEST_EVENT_SPE_STOPPED
    else if (ret > 0) { /* user defined stop & signal */
      barrier_sync(&params->restart, 2);
      tprintf("spe[%d]: restart\n", params->index);
    }
#endif
    else {
      eprintf("spe[%d]: spe_context_run: %s\n", params->index, strerror(errno));
      fatal();
    }
  }

  return NULL;
}

#ifdef ENABLE_TEST_EVENT_TAG_GROUP

static char dma_source[DMA_SIZE] __attribute__((aligned(16)));

static int issue_dma(spe_thread_params_t *params)
{
  int ret;

  if (params->queued_dma >= MFC_PROXY_COMMAND_QUEUE_DEPTH) {
    return 0;
  }

  ret = spe_mfcio_get(params->spe, params->dma_ls,
		      dma_source, DMA_SIZE, params->dma_tag, 0, 0);
  if (ret) {
    eprintf("spe[%d]: spe_mfcio_get: %s\n", params->index, strerror(errno));
    return 1;
  }

  params->queued_dma++;

  return 0;
}
#endif

static int process_event(const spe_event_unit_t *event,
			 spe_thread_params_t *params)
{
  int ret, done;

  done = 0;

  if (event->events & SPE_EVENT_OUT_INTR_MBOX) {
    unsigned int data = -1;
    ret = spe_out_intr_mbox_read(event->spe, &data, 1, SPE_MBOX_ANY_NONBLOCKING);
    if (ret == -1) {
      eprintf("spe_out_intr_mbox_read: %s\n", strerror(errno));
      return -1;
    }
    tprintf("spe[%u]: ibox: 0x%08x(%u)\n", event->data.u32, data, data);
    if (params->num_ibox + 1 != data) {
      eprintf("Unexpected ibox data: %u: Expected: %u\n",
	      data, params->num_ibox + 1);
      return -1;
    }
    params->num_ibox = data;
  }

  if (event->events & SPE_EVENT_TAG_GROUP) {
    unsigned int status;
    tprintf("spe[%u]: Tag Group\n", event->data.u32);
    ret = spe_mfcio_tag_status_read(event->spe, 1 << params->dma_tag,
				    SPE_TAG_IMMEDIATE, &status);
    if (ret) {
      eprintf("spe[%u]: spe_mfcio_tag_status_read: %s\n",
	      event->data.u32, strerror(errno));
      return -1;
    }
    tprintf("spe[%u]: tag: 0x%08x\n", event->data.u32, status);
    params->queued_dma = 0;
  }

  if (event->events & SPE_EVENT_SPE_STOPPED) {
    spe_stop_info_t stop_info;
    tprintf("spe[%u]: Stopped\n", event->data.u32);
    ret = spe_stop_info_read(event->spe, &stop_info);
    if (ret == -1) {
      eprintf("spe[%u]: spe_stop_info_read: %s\n",
	      event->data.u32, strerror(errno));
      return -1;
    }
    if (stop_info.stop_reason == SPE_EXIT) {
      tprintf("spe[%u]: Exited\n", event->data.u32);
      if (stop_info.result.spe_exit_code != EXIT_DATA) {
	eprintf("spe[%u]: Unexpected exit code: %d\n",
		event->data.u32, stop_info.result.spe_exit_code);
	return -1;
      }
      done = 1;
    }
    else if (stop_info.stop_reason == SPE_STOP_AND_SIGNAL) {
      tprintf("spe[%u]: User Signal\n", event->data.u32);
#ifdef ENABLE_TEST_EVENT_SPE_STOPPED
      if (stop_info.result.spe_signal_code != STOP_DATA) {
	eprintf("spe[%u]: Unexpected signal: %d (0x%08x)\n",
		event->data.u32,
		stop_info.result.spe_signal_code,
		stop_info.result.spe_signal_code);
	return -1;
      }
      barrier_sync(&params->restart, 2);
#else
      eprintf("spe[%u]: Unexpected signal: %d (0x%08x)\n",
	      event->data.u32,
	      stop_info.result.spe_signal_code,
	      stop_info.result.spe_signal_code);
      return -1;
#endif
    }
    else if (stop_info.stop_reason == SPE_RUNTIME_EXCEPTION) {
      eprintf("spe[%u]: Runtime Exception: 0x%08x: errno: %d\n",
	      event->data.u32,
	      stop_info.result.spe_runtime_exception,
	      errno);
      return -1;
    }
    else if (stop_info.stop_reason == SPE_RUNTIME_FATAL) {
      eprintf("spe[%u]: Runtime Fatal: %d\n",
	      event->data.u32,
	      stop_info.result.spe_runtime_fatal);
      return -1;
    }
    else if (stop_info.stop_reason == SPE_CALLBACK_ERROR) {
      eprintf("spe[%u]: Callback Error: %d (0x%08x)\n",
	      event->data.u32,
	      stop_info.result.spe_callback_error,
	      stop_info.result.spe_callback_error);
      return -1;
    }
    else {
      eprintf("spe[%u]: Unexpected stop reason: %d\n",
	      event->data.u32,
	      stop_info.stop_reason);
      return -1;
    }
  }

  return done;
}

static int test(int argc, char **argv)
{
  int ret;
  spe_thread_params_t params[NUM_SPES];
  spe_event_handler_ptr_t evhandler;
#define MAX_EVENT (NUM_SPES * 4)
  spe_event_unit_t event[MAX_EVENT];
  int i;
  int exit_count;
  int num_events;

  /* initialize SPE contexts */
  for (i = 0; i < NUM_SPES; i++) {
    params[i].index = i;
    params[i].spe = spe_context_create(SPE_EVENTS_ENABLE, NULL);
    if (!params[i].spe) {
      eprintf("spe_context_create: %s\n", strerror(errno));
      fatal();
    }
    params[i].num_ibox = 0;
    params[i].dma_tag = 3;
    params[i].queued_dma = 0;
#ifdef ENABLE_TEST_EVENT_SPE_STOPPED
    barrier_initialize(&params[i].restart);
#endif
  }

  /* register events */
  evhandler = spe_event_handler_create();
  if (!evhandler) {
    eprintf("spe_event_handler_create: %s\n", strerror(errno));
    fatal();
  }

  for (i = 0; i < NUM_SPES; i++) {
    event[0].events = SPE_EVENT_SPE_STOPPED;
#ifdef ENABLE_TEST_EVENT_OUT_INTR_MBOX
    event[0].events |= SPE_EVENT_OUT_INTR_MBOX;
#endif
#ifdef ENABLE_TEST_EVENT_TAG_GROUP
    event[0].events |= SPE_EVENT_TAG_GROUP;
#endif

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
#ifdef ENABLE_TEST_EVENT_TAG_GROUP
    /* get DMA buffer area */
    ret = spe_out_intr_mbox_read(params[i].spe, &params[i].dma_ls, 1, SPE_MBOX_ANY_BLOCKING);
    if (ret != 1) {
      eprintf("dma_ls: Not available.\n");
      fatal();
    }
    tprintf("spe[%u]: DMA buffer: 0x%08x: Tag: %d\n",
	    i, params[i].dma_ls, params[i].dma_tag);
#endif
  }

#ifdef ENABLE_TEST_EVENT_TAG_GROUP
  /* issue the first DMA request */
  for (i = 0; i < NUM_SPES; i++) {
    ret = issue_dma(params + i);
    if (ret) {
      fatal();
    }
  }
#endif

  exit_count = 0;
  while (exit_count < NUM_SPES) {
    ret = num_events = spe_event_wait(evhandler, event, MAX_EVENT, -1);
    if (ret == -1) {
      eprintf("spe_event_wait: %s\n", strerror(errno));
      fatal();
    }
    else if (ret == 0) {
      eprintf("spe_event_wait: Unexpected timeout.\n");
      fatal();
    }

    for (i = 0; i < num_events; i++) {
      tprintf("event %u/%u: spe[%u]\n", i, num_events, (unsigned int)event->data.u32);
      ret = process_event(event + i, params + event[i].data.u32);
      if (ret < 0) {
	fatal();
      }
      else {
	exit_count += ret;
      }

#ifdef ENABLE_TEST_EVENT_TAG_GROUP
      /* next DMA */
      ret = issue_dma(params + event[i].data.u32);
      if (ret) {
	fatal();
      }
#endif
    }
  }

  ret = num_events = spe_event_wait(evhandler, event, MAX_EVENT, 0);
  if (ret == -1) {
    eprintf("spe_event_wait: %s\n", strerror(errno));
    fatal();
  }
  for (i = 0; i < num_events; i++) {
    tprintf("event %u/%u: spe[%u]\n", i, num_events, (unsigned int)event->data.u32);
    ret = process_event(event + i , params + event[i].data.u32);
    if (ret < 0) {
      fatal();
    }
  }

  for (i = 0; i < NUM_SPES; i++) {
    void *exit_code;

    pthread_join(params[i].tid, &exit_code);

    event[0].spe = params[i].spe;

    event[0].events = SPE_EVENT_SPE_STOPPED;
#ifdef ENABLE_TEST_EVENT_OUT_INTR_MBOX
    event[0].events |= SPE_EVENT_OUT_INTR_MBOX;
#endif
#ifdef ENABLE_TEST_EVENT_TAG_GROUP
    event[0].events |= SPE_EVENT_TAG_GROUP;
#endif

    ret = spe_event_handler_deregister(evhandler, event);
    if (ret == -1) {
      eprintf("spe_event_handler_deregister: %s\n", strerror(errno));
      fatal();
    }

    ret = spe_context_destroy(params[i].spe);
    if (ret) {
      eprintf("spe_context_destroy: %s\n", strerror(errno));
      fatal();
    }
    if (exit_code) {
      fatal();
    }

#ifdef ENABLE_TEST_EVENT_SPE_STOPPED
    barrier_finalize(&params[i].restart);
#endif
  }

  spe_event_handler_destroy(evhandler);

  ret = 0;
#ifdef ENABLE_TEST_EVENT_OUT_INTR_MBOX
  for (i = 0; i < NUM_SPES; i++) {
    if (params[i].num_ibox != IBOX_COUNT) {
      eprintf("spe[%u]: ibox data was lost (%u/%u).\n",
	      i, params[i].num_ibox, IBOX_COUNT);
      ret |= 1;
    }
  }
#endif
  if (ret) {
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
