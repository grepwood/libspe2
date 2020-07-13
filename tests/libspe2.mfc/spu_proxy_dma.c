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

/* This program tells PPE the address of a buffer in LS, and waits
   until the proxy DMA test is finished.
 */

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "spu_libspe2_test.h"

#define BUFFER_SIZE (MAX_DMA_SIZE * 2)

static unsigned char buf[BUFFER_SIZE] __attribute__((aligned(16)));

int main(unsigned long long spe,
	 unsigned long long argp,
	 unsigned long long envp)
{
  /* tell PPE the buffer address */
  spu_write_out_intr_mbox((unsigned int)buf);

  /* wait for notification */
  spu_read_in_mbox();

  return 0;
}
