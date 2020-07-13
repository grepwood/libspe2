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

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "spu_libspe2_test.h"

#define DURATION 3 /* at least, stay in this program for this time (sec). */

#define UNIT 3

unsigned char dma_buffer[MAX_DMA_SIZE] __attribute__((aligned(16)));

int main(unsigned long long spe,
	 unsigned long long argp, unsigned long long envp /* timebase */)
{
  int i;
  unsigned long long dec_sum = 0;
  unsigned long long dec_wait = DURATION * envp;
  unsigned int dec_prev = 0;

#ifdef ENABLE_TEST_EVENT_TAG_GROUP
  spu_write_out_intr_mbox((unsigned int)dma_buffer);
#endif

  spu_write_decrementer(0);

  for (i = 1; i <= argp || dec_sum < dec_wait; ) {
    int j;
    unsigned int dec;
#ifdef ENABLE_TEST_EVENT_SPE_STOPPED
    for (j = 0; j < UNIT; j++) {
      spu_stop(STOP_DATA);
    }
#endif
    for (j = 0; i <= argp && j < UNIT; i++, j++) {
#ifdef ENABLE_TEST_EVENT_OUT_INTR_MBOX
      spu_write_out_intr_mbox(i);
#endif
    }

    dec = spu_read_decrementer();
    dec_sum += (dec_prev - dec);
    dec_prev = dec;
  }

  return EXIT_DATA;
}
