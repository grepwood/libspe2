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

/* This test checks if the direct SPE access functions work
 * correctly.
 */

#include "ppu_libspe2_test.h"

#include <string.h>

static int test(int argc, char **argv)
{
  spe_context_ptr_t spe;
  int ret;
  void *ptr;
  int i;
  const enum ps_area ps_areas[] = {
    SPE_MSSYNC_AREA,
    SPE_MFC_COMMAND_AREA,
    SPE_CONTROL_AREA,
    SPE_SIG_NOTIFY_1_AREA,
    SPE_SIG_NOTIFY_2_AREA,
  };

  /* error (NULL as SPE context) */
  ret = spe_ls_size_get(NULL);
  if (ret >= 0) {
    eprintf("spe_ls_size_get(NULL): Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_ls_size_get(NULL): Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ptr = spe_ls_area_get(NULL);
  if (ptr) {
    eprintf("spe_ls_area_get(NULL): Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_ls_area_get(NULL): Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  ptr = spe_ps_area_get(NULL, SPE_MFC_COMMAND_AREA);
  if (ptr) {
    eprintf("spe_ps_area_get(NULL, SPE_MFC_COMMAND_AREA): Unexpected success.\n");
    fatal();
  }
  if (errno != ESRCH) {
    eprintf("spe_ps_area_get(NULL, SPE_MFC_COMMAND_AREA): Unexpected errno: %d (%s)\n",
	    errno, strerror(errno));
    failed();
  }

  /* success */
  spe = spe_context_create(SPE_MAP_PS, NULL);
  if (!spe) {
    eprintf("spe_context_create(SPE_MAP_PS, NULL): %s\n", strerror(errno));
    fatal();
  }

  ret = spe_ls_size_get(spe);
  if (ret < 0) {
    eprintf("spe_ls_size_get(%p): %s\n", spe, strerror(errno));
    fatal();
  }
  tprintf("LS size: 0x%x(%d)\n", ret, ret);

  ptr = spe_ls_area_get(spe);
  if (!ptr) {
    eprintf("spe_ls_area_get(%p): %s\n", spe, strerror(errno));
    fatal();
  }
  tprintf("LS area: %p\n", ptr);

  for (i = 0; i < sizeof(ps_areas) / sizeof(ps_areas[0]); i++) {
    ptr = spe_ps_area_get(spe, ps_areas[i]);
    if (!ptr) {
      eprintf("spe_ps_area_get(%p, %s): %s\n",
	      spe, ps_area_symbol(ps_areas[i]), strerror(errno));
      fatal();
    }
    tprintf("PS area (%s): %p\n", ps_area_symbol(ps_areas[i]), ptr);
  }

  /* error (Invalid PS area) */
  ptr = spe_ps_area_get(spe, -1);
  if (ptr != NULL) {
    eprintf("spe_ps_area_get(%p, -1): %s\n", spe, strerror(errno));
    fatal();
  }
  if (errno != EINVAL) {
    eprintf("spe_ps_area_get(%p, -1): Unexpected errno: %d (%s)\n",
	    spe, errno, strerror(errno));
    failed();
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    fatal();
  }


  /* error (PS map is disabled) */
  spe = spe_context_create(0, NULL);
  if (!spe) {
    eprintf("spe_context_create(0, NULL): %s\n", strerror(errno));
    fatal();
  }

  for (i = 0; i < sizeof(ps_areas) / sizeof(ps_areas[0]); i++) {
    ptr = spe_ps_area_get(spe, ps_areas[i]);
    if (ptr) {
      eprintf("spe_ps_area_get(%p, %s): Unexpected success.\n",
	      spe, ps_area_symbol(ps_areas[i]));
      fatal();
    }
    if (errno != EACCES) {
      eprintf("spe_ps_area_get(%p, %s): Unexpected errno: %d (%s)\n",
	      spe, ps_area_symbol(ps_areas[i]), errno, strerror(errno));
      failed();
    }
  }

  ret = spe_context_destroy(spe);
  if (ret) {
    eprintf("spe_context_destroy(%p): %s\n", spe, strerror(errno));
    fatal();
  }

  return 0;
}

int main(int argc, char **argv)
{
  return ppu_main(argc, argv, test);
}
