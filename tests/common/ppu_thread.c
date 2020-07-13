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

#include <string.h>

#include "ppu_libspe2_test.h"


/*** barriers ***/
void barrier_initialize(barrier_t *barrier)
{
  barrier->counter = 0;
  barrier->done = 0;
  pthread_mutex_init(&barrier->mutex, 0);
  pthread_cond_init(&barrier->cond, 0);
}

void barrier_finalize(barrier_t *barrier)
{
  pthread_cond_destroy(&barrier->cond);
  pthread_mutex_destroy(&barrier->mutex);
}

void barrier_sync(barrier_t *barrier, int thread_count)
{
  pthread_mutex_lock(&barrier->mutex);

  /* wait until all threads leave the previous sync point */
  while (barrier->done) {
    pthread_cond_wait(&barrier->cond, &barrier->mutex);
  }

  barrier->counter++;
  if (barrier->counter == thread_count) {
    /* this thread is the last one which comes to the sync point */
    barrier->done = thread_count - 1;
    barrier->counter = 0;
    pthread_cond_broadcast(&barrier->cond);
  }
  else {
    /* wait until all threads reach the sync point */
    while (!barrier->done) {
      pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }

    ASSERT(barrier->counter == 0);

    barrier->done--;
    if (barrier->done == 0) {
      /* this thread is the last one which leave the sync point */
      pthread_cond_broadcast(&barrier->cond);
    }
  }

  pthread_mutex_unlock(&barrier->mutex);
}

static barrier_t g_barrier = BARRIER_INITIALIZER;

void global_sync(int thread_count)
{
  barrier_sync(&g_barrier, thread_count);
}

/*** multi-threading helpers ***/
ppe_thread_group_t *ppe_thread_group_create(size_t num_threads,
					    void *(*proc)(void *), void *data)
{
  int i;
  ppe_thread_group_t *group = malloc(sizeof(ppe_thread_group_t));
  if (!group) {
    eprintf("malloc: %s\n", strerror(errno));
    return NULL;
  }

  group->threads = calloc(num_threads, sizeof(ppe_thread_t));
  if (!group->threads) {
    eprintf("calloc: %s\n", strerror(errno));
    free(group);
    return NULL;
  }

  group->data = data;
  group->nr_threads = num_threads;

  /* create PPU threads */
  for (i = 0; i < num_threads; i++) {
    int ret;
    group->threads[i].index = i;
    group->threads[i].group = group;
    ret = pthread_create(&group->threads[i].thread, NULL, proc, group->threads + i);
    if (ret) {
      eprintf("pthread_create: %s\n", strerror(ret));
      return NULL;
    }
  }

  return group;
}

int ppe_thread_group_wait(ppe_thread_group_t *group, void **exit_codes)
{
  int i;

  /* wait for all PPU threads */
  for (i = 0; i < group->nr_threads; i++) {
    pthread_join(group->threads[i].thread, exit_codes ? exit_codes + i : NULL);
  }

  free(group->threads);
  free(group);

  return 0;
}

int ppe_thread_group_run(size_t num_threads, void *(*proc)(void *), void *data,
			 void **exit_codes)
{
  ppe_thread_group_t *group = ppe_thread_group_create(num_threads, proc, data);
  if (!group) return -1;

  return ppe_thread_group_wait(group, exit_codes);
}
