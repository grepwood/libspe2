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

#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#include "spu_libspe2_test.h"

#define SPIN_COUNT 10000

int main(unsigned long long spe,
	 unsigned long long argp, unsigned long long envp)
{
  unsigned long long i;
  int j;

  for (i = 0; i < argp; i++) {
    unsigned int n;

    /*** inbound mailbox tests. Read one of MAX_WBOX_COUNT data. ***/
    n = spu_read_in_mbox();

    /*** outbound mailbox tests. Write one of MAX_MBOX_COUNT data to
     * make sure there is at least one data at the first
     * spe_out_mbox_read call on PPE side.
     ***/
    spu_write_out_mbox(n + 2);

    /*** outbound interrupt mailbox tests. Write IBOX_DEPTH data. ***/

    /* delay intentionally to increase probability of waiting on
       outbound interrupt mailbox on PPE side */
    spin(SPIN_COUNT);
    /* write to outbound interrupt mailbox */
    for (j = 0; j < IBOX_DEPTH; j++) {
      spu_write_out_intr_mbox(n + 1 + j);
    }

    /*** output mailbox tests. Write remainder. ***/
    for (j = 1; j < MAX_MBOX_COUNT; j++) {
      spu_write_out_mbox(n + 2 + j);
    }

    /* delay intentionally to increase probability of waiting on
       inbound mailbox on PPE side in the next iteration */
    spin(SPIN_COUNT);

    /* read remainder from inbound mailbox. */
    for (j = 1; j < WBOX_DEPTH; j++) {
      unsigned int m = spu_read_in_mbox();
      if (m != n + j) { /* unexpected data */
	return 1;
      }
    }
  }

  return 0;
}
