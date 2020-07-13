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

/* This program issues an invalid DMA command intentionally so that an
 * invalid DMA error is raised.
 */

#include "spu_libspe2_test.h"
#include <spu_mfcio.h>

#define TAG 1

unsigned g_dma_buffer[MAX_DMA_SIZE * 2] __attribute__((aligned(16)));

int main(unsigned long long spe,
	 unsigned long long argp /* EA */,
	 unsigned long long envp)
{
  /* this operation should be failed */
  spu_mfcdma64(g_dma_buffer, mfc_ea2h(argp), mfc_ea2l(argp),
	       MAX_DMA_SIZE, TAG, MFC_INVALID_COMMAND_OPCODE);
  mfc_write_tag_mask(1 << TAG);
  mfc_read_tag_status_all();

  return 0;
}
