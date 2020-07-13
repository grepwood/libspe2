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
#include <stdarg.h>
#include <string.h>

#include "spu_c99_test.h"

DEFINE_SPE_ERROR_INFO;

#define BUFFER_SIZE (1024 * 4)

static int vscanf_test(const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vscanf(fmt, va);
  va_end(va);

  return ret;
}

static int vprintf_test(const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vprintf(fmt, va);
  va_end(va);

  return ret;
}

int main()
{
  PRINTF_TEST_VARS_DECL;

  int ch;
  char buf[BUFFER_SIZE];

  /* getchar and putchar */
  ch = getchar();
  if (ch == EOF) {
    error_info_exit(1);
  }
  ch = putchar(ch);
  if (ch == EOF) {
    error_info_exit(1);
  }
  if (putchar('\n') == EOF) {
    error_info_exit(1);
  }


  /* gets and puts */
  if (gets(buf) != buf) {
    error_info_exit(1);
  }
  if (puts(buf) == EOF) {
    error_info_exit(1);
  }


  /* scanf */
  SCANF_TEST_INIT_VARS;
  if (scanf(SCANF_TEST_FORMAT, SCANF_TEST_ARGS) != SCANF_TEST_NUM) {
    error_info_exit(1);
  }
  if (!SCANF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* printf */
  if (printf("") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  if (printf(PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* vscanf */
  SCANF_TEST_INIT_VARS;
  if (vscanf_test(SCANF_TEST_FORMAT, SCANF_TEST_ARGS) != SCANF_TEST_NUM) {
    error_info_exit(1);
  }
  if (!SCANF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* vprintf */
  if (vprintf_test("") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  if (vprintf_test(PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }

  return 0;
}
