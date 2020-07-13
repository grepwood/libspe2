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

static int vsscanf_test(const char *str, const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vsscanf(str, fmt, va);
  va_end(va);

  return ret;
}

static int vsprintf_test(char *str, const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vsprintf(str, fmt, va);
  va_end(va);

  return ret;
}

static int vsnprintf_test(char *str, size_t size, const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vsnprintf(str, size, fmt, va);
  va_end(va);

  return ret;
}

int main()
{
  PRINTF_TEST_VARS_DECL;

  char buf[BUFFER_SIZE];

  /* sscanf */
  SCANF_TEST_INIT_VARS;
  if (sscanf(SCANF_TEST_STRING, SCANF_TEST_FORMAT, SCANF_TEST_ARGS) != SCANF_TEST_NUM) {
    error_info_exit(1);
  }
  if (!SCANF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* vsscanf */
  SCANF_TEST_INIT_VARS;
  if (vsscanf_test(SCANF_TEST_STRING, SCANF_TEST_FORMAT, SCANF_TEST_ARGS) != SCANF_TEST_NUM) {
    error_info_exit(1);
  }
  if (!SCANF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* sprintf */
  memset(buf, -1, sizeof(buf));
  if (sprintf(buf, "") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  memset(buf, -1, sizeof(buf));
  if (sprintf(buf, PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }
  if (strcmp(buf, PRINTF_TEST_STRING)) {
    error_info_exit(1);
  }

  /* vsprintf */
  memset(buf, -1, sizeof(buf));
  if (vsprintf_test(buf, "") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  memset(buf, -1, sizeof(buf));
  if (vsprintf_test(buf, PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }
  if (strcmp(buf, PRINTF_TEST_STRING)) {
    error_info_exit(1);
  }

  /* snprintf */
  memset(buf, -1, sizeof(buf));
  if (snprintf(buf, sizeof(buf), "") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  memset(buf, -1, sizeof(buf));
  if (snprintf(buf, sizeof(buf), PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }
  if (strcmp(buf, PRINTF_TEST_STRING)) {
    error_info_exit(1);
  }

  /* vsnprintf */
  memset(buf, -1, sizeof(buf));
  if (vsnprintf_test(buf, sizeof(buf), "") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  memset(buf, -1, sizeof(buf));
  if (vsnprintf_test(buf, sizeof(buf), PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }
  if (strcmp(buf, PRINTF_TEST_STRING)) {
    error_info_exit(1);
  }

  return 0;
}
