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

/* This test checks if the 'poll' operation on proxy DMA node ("mfc")
 * of SPUFS works correctly. Essentially, this is not for libspe,
 * however, this is useful for SPUFS system test.
 */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/epoll.h>

#include "ppu_libspe2_test.h"

#define COUNT 10000
#define DMA_SIZE MAX_DMA_SIZE

#define USE_EPOLL 1
//#define USE_PS 1

extern spe_program_handle_t spu_proxy_dma;

typedef struct spe_thread_params
{
  spe_context_ptr_t spe;
  ppe_thread_t *ppe;
} spe_thread_params_t;

static void *spe_thread_proc(void *arg)
{
  spe_thread_params_t *params = (spe_thread_params_t *)arg;
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_stop_info_t stop_info;

  if (spe_program_load(params->spe, &spu_proxy_dma)) {
    eprintf("spe_program_load(%p, &spu_proxy_dma): %s\n", params->spe, strerror(errno));
    fatal();
  }

  ret = spe_context_run(params->spe, &entry, 0, NULL, NULL, &stop_info);
  if (ret == 0) {
    if (check_exit_code(&stop_info, 0)) {
      fatal();
    }
  }
  else {
    eprintf("spe_context_run(%p): %s\n", params->spe, strerror(errno));
    fatal();
  }

  return NULL;
}

static void *ppe_thread_proc(void *arg)
{
  ppe_thread_t *ppe = (ppe_thread_t*)arg;
  spe_thread_params_t spe_params;
  pthread_t tid;
  int ret;
  int i;
  int mfc_fd;
  void *buf;
  unsigned int ls_dma_buf;
#ifdef USE_EPOLL
  int epfd;
  struct epoll_event ep_event;
#endif

  spe_params.ppe = ppe;
#ifdef USE_PS
  spe_params.spe = spe_context_create(SPE_MAP_PS, 0);
#else
  spe_params.spe = spe_context_create(0, 0);
#endif
  if (!spe_params.spe) {
    eprintf("spe_context_create: %s\n", strerror(errno));
    fatal();
  }

  mfc_fd = spufs_open(spe_params.spe, "mfc", O_RDWR);
  if (mfc_fd == -1) {
    eprintf("%ld: %s: %s\n", (unsigned long)spe_params.spe, "mfc", strerror(errno));
    fatal();
  }

  ret = posix_memalign(&buf, 0x10000, DMA_SIZE);
  if (ret) {
    eprintf("%ld: %s: %s\n", (unsigned long)spe_params.spe, "mfc", strerror(ret));
    fatal();
  }

  ret = pthread_create(&tid, NULL, spe_thread_proc, &spe_params);
  if (ret) {
    eprintf("pthread_create: %s\n", strerror(ret));
    fatal();
  }

#if 0
  ls_dma_buf = 0;
#else
  /* To guarantee SPE context has started.
   *
   * FIXME: This test must work w/o this workaround, however, it is
   * needed now because of some bugs in SPUFS on "mfc" node.
   */
  ret = spe_out_intr_mbox_read(spe_params.spe, &ls_dma_buf, 1, SPE_MBOX_ANY_BLOCKING);
  if (ret != 1) {
    eprintf("dma_buffer: Not available.\n");
    fatal();
  }
#endif

#ifdef USE_EPOLL
  epfd = epoll_create(10);
  if (epfd == -1) {
    eprintf("epoll_create: %s\n", strerror(errno));
    fatal();
  }

  ep_event.events = EPOLLIN;
  if (epoll_ctl(epfd, EPOLL_CTL_ADD, mfc_fd, &ep_event) == -1) {
    eprintf("epoll_ctl: %s\n", strerror(errno));
    fatal();
  }
#endif

  /* synchronize all PPE threads */
  global_sync(NUM_SPES);

  for (i = 0; i < COUNT; i++) {
    unsigned int tag = (i % MFC_PROXY_COMMAND_QUEUE_DEPTH) + 1;

    ret = spe_mfcio_put(spe_params.spe, ls_dma_buf, buf, DMA_SIZE, tag, 0, 0);
    if (ret) {
      eprintf("spe_mfcio_put(%u/%u): %s\n", i, COUNT, strerror(errno));
      fatal();
    }

    for ( ; ; ) {
      tprintf("waiting\n");
#ifndef USE_PS
#ifdef USE_EPOLL
      ret = epoll_wait(epfd, &ep_event, 1, -1);
#else
      struct pollfd fds;
      fds.fd = mfc_fd;
      fds.events = POLLIN;
      ret = poll(&fds, 1, -1);
      if (ret == -1) {
	eprintf("poll: %s\n", strerror(errno));
	fatal();
      }
#endif
      tprintf("done\n");
      if (ret == -1) {
	eprintf("epoll_wait/poll: %s\n", strerror(errno));
	fatal();
      }
      else if (ret == 0) {
	eprintf("epoll_wait/poll: No data\n");
	fatal();
      }
#ifdef USE_EPOLL
      else if (ep_event.events & EPOLLIN)
#else
      else if (fds.revents & POLLIN)
#endif
	{
#endif /* !USE_PS */
	  unsigned int mask = 1 << tag;
	  unsigned int tag_status = 0;
	  int behavior;
#ifdef USE_PS
	  behavior = SPE_TAG_ANY;
#else
	  behavior = SPE_TAG_IMMEDIATE;
#endif
	  tprintf("reading\n");
	  ret = spe_mfcio_tag_status_read(spe_params.spe, mask, behavior, &tag_status);
	  tprintf("done\n");
	  if (ret) {
	    eprintf("spe_mfcio_tag_status_read: %s\n", strerror(errno));
	    fatal();
	  }
	  if (tag_status != mask) {
	    eprintf("spe_mfcio_tag_status_read: unexpected tag status 0x%08x\n", tag_status);
	    fatal();
	  }
	  break;
#ifndef USE_PS
	}
      else {
	eprintf("epoll_wait: unexpected event.\n");
	fatal();
      }
#endif /* !USE_PS */
    }
  }

  /* notify test has finished */
  ret = spe_in_mbox_write(spe_params.spe, (unsigned int *)buf, 1, SPE_MBOX_ALL_BLOCKING);
  if (ret == -1) {
    eprintf("spe_in_mbox_write: %s\n", strerror(errno));
    fatal();
  }

  pthread_join(tid, NULL);

#ifdef USE_EPOLL
  close(epfd);
#endif
  close(mfc_fd);

  ret = spe_context_destroy(spe_params.spe);
  if (ret) {
    eprintf("spe_context_destroy: %s\n", strerror(errno));
    fatal();
  }

  free(buf);

  return NULL;
}

static int test(int argc, char **argv)
{
  int ret;

  ret = ppe_thread_group_run(NUM_SPES, ppe_thread_proc, NULL, NULL);
  if (ret) {
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
