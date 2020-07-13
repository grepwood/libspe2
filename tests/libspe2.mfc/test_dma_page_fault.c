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

/* This test checks if DMA (not proxy DMA) is performed correctly with
 * intentional frequent page faults. Essentially, this is not for
 * libspe test, however, this is useful for SPUFS system test.
 */

#include "ppu_libspe2_test.h"

#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <sys/mman.h>

#define COUNT 300
#define DMA_SIZE MAX_DMA_SIZE
#define BUFFER_SIZE (DMA_SIZE * MFC_COMMAND_QUEUE_DEPTH)

extern spe_program_handle_t spu_dma;

static char dma_data[BUFFER_SIZE];

typedef struct spe_thread_params
{
  spe_context_ptr_t spe;
  ppe_thread_t *ppe;
} spe_thread_params_t;

static void *spe_thread_proc(void *arg)
{
  int ret;
  int map_fd;
  char map_file[256];
  int i;

  map_fd = tmpfile_open(map_file, "dma");
  if (map_fd == -1) {
    eprintf("tmpfile_open(%s, \"dma\"): %s\n", map_file, strerror(errno));
    fatal();
  }
  ret = write(map_fd, dma_data, BUFFER_SIZE);
  if (ret == -1) {
    eprintf("write(%d, %p, %s): %s\n", map_fd, dma_data, map_file, strerror(errno));
    fatal();
  }
  if (ret != BUFFER_SIZE) {
    eprintf("write incomplete: %s\n", map_file);
    fatal();
  }

  for (i = 0; i < COUNT; i++) {
    unsigned int entry = SPE_DEFAULT_ENTRY;
    spe_context_ptr_t spe;
    spe_stop_info_t stop_info;
    void *map;

    map = mmap(NULL, BUFFER_SIZE, PROT_READ, MAP_PRIVATE, map_fd, 0);
    if (map == MAP_FAILED) {
      eprintf("mmap(NULL, ...): %s\n", strerror(errno));
      fatal();
    }

    spe = spe_context_create(0, 0);
    if (!spe) {
      eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
      fatal();
    }

    if (spe_program_load(spe, &spu_dma)) {
      eprintf("spe_program_load(%p, &spu_dma): %s\n", spe, strerror(errno));
      fatal();
    }

    /* synchronize all threads */
    global_sync(NUM_SPES);

    ret = spe_context_run(spe, &entry, 0, (void*)COUNT, map, &stop_info);
    if (ret == 0) {
      if (check_exit_code(&stop_info, 0)) {
	fatal();
      }
    }
    else {
      eprintf("spe_context_run(%p, ...): %s\n", spe, strerror(errno));
      fatal();
    }

    ret = spe_context_destroy(spe);
    if (ret) {
      eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
      fatal();
    }

    munmap(map, BUFFER_SIZE);
  }

  close(map_fd);
  unlink(map_file);

  return NULL;
}

static int test(int argc, char **argv)
{
  int ret;

  generate_data(dma_data, 0, sizeof(dma_data));

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
