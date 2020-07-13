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

/* This program accesses the outbound interrupt mailbox and executes
 * stop instruction almost at the same time.
 */

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "spu_libspe2_test.h"

#define UNIT 3

int main(unsigned long long spe,
	 unsigned long long argp /* count */,
	 unsigned long long envp)
{
  int i;

  for (i = 1; i <= argp; ) {
    int j;
    for (j = 0; j < UNIT && i <= argp; j++, i++) {
      spu_write_out_intr_mbox(i);
    }
    for (j = 0; j < UNIT; j++) {
      spu_stop(1);
    }
  }

  return 0;
}
