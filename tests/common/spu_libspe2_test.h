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

/* This file provides definitions and declarations used by SPEs. */

#ifndef __SPU_LIBSPE2_TEST_H
#define __SPU_LIBSPE2_TEST_H

#include "common_libspe2_test.h"

#include <spu_intrinsics.h>
#include <errno.h>
#include <stdlib.h>

/* Check if data is correct. It's expected that the source data is
 * created by PPE's 'data_generate' function.
 *
 * idx must be a product of 4.
 * size must be a product of 16.
 */
static inline int data_check(unsigned char *p, unsigned int idx, int size)
{
  vector unsigned int *pv = (vector unsigned int *)p;
  vector unsigned int *pv_end = pv + (size >> 4);
  unsigned int init = idx >> 2;
  vector unsigned int values = { init, init + 1, init + 2, init + 3, };

  while (pv < pv_end) {
    if (spu_extract(spu_gather(spu_cmpeq(values, *pv)), 0) != 0xf) {
      return 0;
    }
    values += spu_splats(4U);
    pv++;
  }
  return 1;
}

/* Spin specified times. */
static inline void spin(unsigned long long n)
{
  volatile unsigned long long i = n;
  while (i-- > 0) {
    /* dummy loop */
  }
}

/*** To locate the g_spe_error_info at the fixed LS address so that
 *   the PPE code can access it easily, we define the variable in the
 *   ".interrupt" section, which always starts at '0'.
 ***/
#define DEFINE_SPE_ERROR_INFO \
  volatile spe_error_info_t g_spe_error_info __attribute__((section(".interrupt")))
#define DECLARE_SPE_ERROR_INFO extern volatile spe_error_info_t g_spe_error_info

#define error_info_exit(n) do {				 \
    g_spe_error_info.code = (n);			 \
    g_spe_error_info.file = (unsigned int)__FILE__;	 \
    g_spe_error_info.line = __LINE__;			 \
    g_spe_error_info.err = errno;			 \
    exit(n);						 \
  } while (0)

#endif /* __SPU_LIBSPE2_TEST_H */
