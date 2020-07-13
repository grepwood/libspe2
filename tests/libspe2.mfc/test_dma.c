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

/* This test checks if DMA (not proxy DMA) is performed
 * correctly. Essentially, this is not for libspe test, however, this
 * is useful for SPUFS system test.
 */

#include "ppu_libspe2_test.h"

#include <stdint.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define COUNT 10000
#define DMA_SIZE MAX_DMA_SIZE
#define BUFFER_SIZE (DMA_SIZE * MFC_COMMAND_QUEUE_DEPTH)

extern spe_program_handle_t spu_dma;

static char dma_data[BUFFER_SIZE] __attribute__((aligned(16)));

static void *spe_thread_proc(void *arg)
{
  unsigned int entry = SPE_DEFAULT_ENTRY;
  int ret;
  spe_context_ptr_t spe;
  spe_stop_info_t stop_info;

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

  ret = spe_context_run(spe, &entry, 0, (void*)COUNT, dma_data, &stop_info);
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
