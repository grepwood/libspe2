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

/* This test checks if the library can handle mailboxes-related errors
 * correctly.
 */

#include "ppu_libspe2_test.h"
#include <errno.h>
#include <string.h>

static int test(int argc, char **argv)
{
  int ret;
  unsigned int mbox_data[100];
  spe_context_ptr_t spe;

  spe = spe_context_create(0, 0);
  if (!spe) {
    eprintf("spe_context_create: %s\n", strerror(errno));
    fatal();
  }

  /* spe_out_mbox_read */
  ret = spe_out_mbox_read(NULL, mbox_data, 1);
  if (ret != -1) {
    eprintf("spe_out_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_out_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_out_mbox_read(spe, NULL, 1);
  if (ret != -1) {
    eprintf("spe_out_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_out_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_out_mbox_read(spe, NULL, -1);
  if (ret != -1) {
    eprintf("spe_out_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_out_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* spe_in_mbox_write */
  ret = spe_in_mbox_write(NULL, mbox_data, 1, SPE_MBOX_ANY_BLOCKING);
  if (ret != -1) {
    eprintf("spe_in_mbox_write: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_in_mbox_write: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_in_mbox_write(spe, NULL, 1, SPE_MBOX_ANY_BLOCKING);
  if (ret != -1) {
    eprintf("spe_in_mbox_write: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_in_mbox_write: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_in_mbox_write(spe, mbox_data, -1, SPE_MBOX_ANY_BLOCKING);
  if (ret != -1) {
    eprintf("spe_in_mbox_write: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_in_mbox_write: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_in_mbox_write(spe, mbox_data, 1, -1);
  if (ret != -1) {
    eprintf("spe_in_mbox_write: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_in_mbox_write: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* spe_out_intr_mbox_read */
  ret = spe_out_intr_mbox_read(NULL, mbox_data, 1, SPE_MBOX_ANY_BLOCKING);
  if (ret != -1) {
    eprintf("spe_out_intr_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_out_intr_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_out_intr_mbox_read(spe, NULL, 1, SPE_MBOX_ANY_BLOCKING);
  if (ret != -1) {
    eprintf("spe_out_intr_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_out_intr_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_out_intr_mbox_read(spe, mbox_data, -1, SPE_MBOX_ANY_BLOCKING);
  if (ret != -1) {
    eprintf("spe_out_intr_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_out_intr_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_out_intr_mbox_read(spe, mbox_data, 1, -1);
  if (ret != -1) {
    eprintf("spe_out_intr_mbox_read: Unexpected success.\n");
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_out_intr_mbox_read: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* spe_*_mbox_status */
  ret = spe_out_mbox_status(NULL);
  if (ret != -1) {
    eprintf("spe_out_mbox_status: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_out_mbox_status: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_in_mbox_status(NULL);
  if (ret != -1) {
    eprintf("spe_in_mbox_status: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_in_mbox_status: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ret = spe_out_intr_mbox_status(NULL);
  if (ret != -1) {
    eprintf("spe_out_intr_mbox_status: Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_out_intr_mbox_status: Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* clean up */
  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy: %s\n", strerror(errno));
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
