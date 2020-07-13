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

/* NOTE:
 *   Don't call 'stdio' functions to display error messages!!!
 *   This is a test of stdio!
 */

#include <stdio.h>

#include "spu_c99_test.h"

DEFINE_SPE_ERROR_INFO;

#define INPUT_FILE "__c99_stdio_file_input.tmp"
#define OUTPUT_FILE "__c99_stdio_file_result.tmp"

extern int main_test(FILE *ifp, FILE *ofp);
extern int main_seekable_test(FILE *ifp, FILE *ofp);
extern int main_tmp_test(FILE *tmpfp, FILE *ifp, FILE *ofp);

int main()
{
  int ret;
  FILE *ifp, *ofp;
  FILE *tmpfp;

  ifp = fopen(INPUT_FILE, "r");
  if (!ifp) {
    error_info_exit(1);
  }
  ofp = fopen(OUTPUT_FILE, "w");
  if (!ofp) {
    error_info_exit(1);
  }

  ret = main_seekable_test(ifp, ofp);
  if (ret) {
    error_info_exit(1);
  }

  tmpfp = tmpfile();
  if (!tmpfp) {
    error_info_exit(1);
  }

  if (main_tmp_test(tmpfp, ifp, ofp)) {
    error_info_exit(1);
  }

  ifp = freopen(INPUT_FILE, "r", stdin);
  if (!ifp) {
    error_info_exit(1);
  }

  ret = main_test(stdin, ofp);
  if (ret) {
    error_info_exit(1);
  }

  if (fclose(ifp)) {
    error_info_exit(1);
  }
  if (fclose(ofp)) {
    error_info_exit(1);
  }

  return 0;
}
