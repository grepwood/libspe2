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

/* This test checks if the proxy DMA support works correctly. */

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#include "ppu_libspe2_test.h"

#define COUNT 2000
#define DMA_SIZE MAX_DMA_SIZE
#define MAX_SIZE (DMA_SIZE * 2)

extern spe_program_handle_t spu_proxy_dma;

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

static unsigned char data_buf[MAX_SIZE];


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
  test_params_t *params = (test_params_t *)ppe->group->data;
  int ret;
  int i;
  int map_fd;
  void *map;
  char map_file[256];
  void *ls_ea;
  static const unsigned char zero_buf[MAX_SIZE];
  unsigned int ls_addr;

  spe_params.ppe = ppe;
  spe_params.spe = spe_context_create(params->flags, 0);
  if (!spe_params.spe) {
    eprintf("%s: spe_context_create: %s\n", params->name, strerror(errno));
    fatal();
  }

  ret = pthread_create(&tid, NULL, spe_thread_proc, &spe_params);
  if (ret) {
    eprintf("%s: pthread_create: %s\n", params->name, strerror(ret));
    fatal();
  }

  /* Initialize data.
     We use mmap() to increase probability of page faults. */
  map_fd = tmpfile_open(map_file, "proxy");
  if (map_fd == -1) {
    eprintf("%s: tmpfile_open: %s\n", params->name, strerror(errno));
    fatal();
  }

  ls_ea = spe_ls_area_get(spe_params.spe);
  if (!ls_ea) {
    eprintf("%s: spe_ls_area_get: %s\n", params->name, strerror(errno));
    fatal();
  }

  /* get LS address via outbound interrupt mailbox */
  ret = spe_out_intr_mbox_read(spe_params.spe, &ls_addr, 1, SPE_MBOX_ALL_BLOCKING);
  if (ret != 1) {
    eprintf("%s: spe_out_intr_mbox_read: %s\n", params->name, strerror(errno));
    fatal();
  }

  /* synchronize all PPE threads */
  global_sync(NUM_SPES);

  for (i = 0; i < COUNT; i++) {
    unsigned int tag, tag_mask;
    unsigned int tag_status;
    unsigned int behavior;
    unsigned int retry, retry_count;
    unsigned char *ptr;
    int put_p;
    int data_offset;

    put_p = (i & 1);
    data_offset = (((i >> 1) << 4) & 0xff);

    /* initialize contents of mapped file */
    if (lseek(map_fd, 0, SEEK_SET) == 1) {
      eprintf("%s: lseek(%s): %s\n", params->name, map_file, strerror(errno));
      fatal();
    }

    ret = write(map_fd, put_p ? zero_buf : data_buf, MAX_SIZE);
    if (ret == -1) {
      eprintf("write(%s): %s\n", map_file, strerror(errno));
      fatal();
    }
    if (ret != MAX_SIZE) {
      eprintf("%s: write incomplete: %s\n", params->name, map_file);
      fatal();
    }

    map = mmap(NULL, MAX_SIZE, PROT_READ | (put_p ? PROT_WRITE : 0), MAP_PRIVATE, map_fd, 0);
    if (map == MAP_FAILED) {
      eprintf("%s: mmap(%s): %s\n", params->name, map_file, strerror(errno));
      fatal();
    }

    ptr = map + data_offset; /* EA */

    tag = i & 0xf; /* TAG */

    /* issue DMA */
    if (put_p) {
      switch ((i >> 1) % 4) {
      case 0:
	ret = spe_mfcio_putb(spe_params.spe, ls_addr, ptr, DMA_SIZE, tag, 0, 0);
	break;
      case 1:
	ret = spe_mfcio_putf(spe_params.spe, ls_addr, ptr, DMA_SIZE, tag, 0, 0);
	break;
      default:
	ret = spe_mfcio_put(spe_params.spe, ls_addr, ptr, DMA_SIZE, tag, 0, 0);
	break;
      }
    }
    else {
      switch ((i >> 1) % 4) {
      case 0:
	ret = spe_mfcio_getb(spe_params.spe, ls_addr, ptr, DMA_SIZE, tag, 0, 0);
	break;
      case 1:
	ret = spe_mfcio_getf(spe_params.spe, ls_addr, ptr, DMA_SIZE, tag, 0, 0);
	break;
      default:
	ret = spe_mfcio_get(spe_params.spe, ls_addr, ptr, DMA_SIZE, tag, 0, 0);
	break;
      }
    }
    if (ret) {
      eprintf("%s: spe_mfcio_get/put: %s\n", params->name, strerror(errno));
      fatal();
    }

    if (i & 0x10) {
      tag_mask = 1 << tag; /* test for tagmask != 0 */
    }
    else {
      tag_mask = 0; /* test for tagmask == 0 */
    }

    /* choose behavior */
    switch (i % 3) {
    case 0:
      behavior = SPE_TAG_ANY;
      break;
    case 2:
      behavior = SPE_TAG_ALL;
      break;
    default:
      behavior = SPE_TAG_IMMEDIATE;
      break;
    }

    /* wait for completion */
    retry_count = 0;
    do {
      ret = spe_mfcio_tag_status_read(spe_params.spe, tag_mask, behavior, &tag_status);
      if (ret) {
	eprintf("%s: spe_mfcio_tag_status_read(%s): %s\n",
		params->name, proxy_dma_behavior_symbol(behavior), strerror(errno));
	fatal();
      }
      if (!(tag_status & (1 << tag))) {
	if (behavior == SPE_TAG_IMMEDIATE) {
	  retry_count++;
	  retry = 1;
	}
	else {
	  eprintf("%s: spe_mfcio_tag_status_read(%s): DMA not completed.\n",
		  params->name, proxy_dma_behavior_symbol(behavior));
	  fatal();
	}
      }
      else {
	retry = 0;
      }
    } while (retry);

    if (!(tag_status & (1 << tag))) {
      eprintf("%s: %s: DMA not completed.\n",
	      params->name, proxy_dma_behavior_symbol(behavior));
      fatal();
    }
    if (retry_count) {
      tprintf("%s: %s: Tag status: Retry %d times.\n",
	      params->name, proxy_dma_behavior_symbol(behavior), retry_count);
    }

    /* check result */
    if (memcmp(put_p ? ptr : ls_ea + ls_addr, data_buf + data_offset, DMA_SIZE)) {
      eprintf("%s: DMA result mismatch.\n", params->name);
      fatal();
    }

    munmap(map, MAX_SIZE);
  }

  /* notify test has finished */
  ret = spe_in_mbox_write(spe_params.spe, (unsigned int *)zero_buf, 1, SPE_MBOX_ALL_BLOCKING);
  if (ret == -1) {
    eprintf("%s: spe_in_mbox_write: %s\n", params->name, strerror(errno));
    fatal();
  }

  pthread_join(tid, NULL);
  ret = spe_context_destroy(spe_params.spe);
  if (ret) {
    eprintf("%s: spe_context_destroy: %s\n", params->name, strerror(errno));
    fatal();
  }

  close(map_fd);
  unlink(map_file);

  return NULL;
}

static int test(int argc, char **argv)
{
  int ret;
  test_params_t params;

  generate_data(data_buf, 0, MAX_SIZE);

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
