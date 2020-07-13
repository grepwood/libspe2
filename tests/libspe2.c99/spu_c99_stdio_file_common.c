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

DECLARE_SPE_ERROR_INFO;

#define BUFFER_SIZE (1024 * 4)

static int vfscanf_test(FILE *fp, const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vfscanf(fp, fmt, va);
  va_end(va);

  return ret;
}

static int vfprintf_test(FILE *fp, const char *fmt, ...)
{
  int ret;
  va_list va;

  va_start(va, fmt);
  ret = vfprintf(fp, fmt, va);
  va_end(va);

  return ret;
}

static int main_test_1(FILE *ifp, FILE *ofp)
{
  int c;

  /* getc, putc */
  c = getc(ifp);
  if (c == EOF) {
    error_info_exit(1);
  }
  if (putc(c, ofp) == EOF) {
    error_info_exit(1);
  }
  if (putc('\n', ofp) == EOF) {
    error_info_exit(1);
  }

  return 0;
}

static int main_test_2(FILE *ifp, FILE *ofp)
{
  PRINTF_TEST_VARS_DECL;

  int ch;
  char buf[BUFFER_SIZE];

  ch = fgetc(ifp);
  if (ch == EOF) {
    error_info_exit(1);
  }
  if (fputc(ch, ofp) == EOF) {
    error_info_exit(1);
  }
  if (fputc('\n', ofp) == EOF) {
    error_info_exit(1);
  }

  /* ungetc */
  if (ungetc(ch, ifp) != ch) {
    error_info_exit(1);
  }
  ch = fgetc(ifp);
  if (ch == EOF) {
    error_info_exit(1);
  }
  if (fputc(ch, ofp) == EOF) {
    error_info_exit(1);
  }
  if (fputc('\n', ofp) == EOF) {
    error_info_exit(1);
  }

  /* fgets and fputs */
  if (fgets(buf, sizeof(buf), ifp) != buf) {
    error_info_exit(1);
  }
  if (fputs(buf, ofp) == EOF) {
    error_info_exit(1);
  }

  /* fread and fwrite */
  if (fread(buf, 11, 1, ifp) != 1) {
    error_info_exit(1);
  }
  if (fwrite(buf, 11, 1, ofp) != 1) {
    error_info_exit(1);
  }

  /* fflush */
  if (fflush(ofp) != 0) {
    error_info_exit(1);
  }

  /* fscanf */
  SCANF_TEST_INIT_VARS;
  if (fscanf(ifp, SCANF_TEST_FORMAT, SCANF_TEST_ARGS) != SCANF_TEST_NUM) {
    error_info_exit(1);
  }
  if (!SCANF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* fprintf */
  if (fprintf(ofp, "") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  if (fprintf(ofp, PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* vfscanf */
  SCANF_TEST_INIT_VARS;
  if (vfscanf_test(ifp, SCANF_TEST_FORMAT, SCANF_TEST_ARGS) != SCANF_TEST_NUM) {
    error_info_exit(1);
  }
  if (!SCANF_TEST_CHECK) {
    error_info_exit(1);
  }

  /* vfprintf */
  if (vfprintf_test(ofp, "") != 0) {
    error_info_exit(1);
  }

  PRINTF_TEST_INIT_VARS;
  if (vfprintf_test(ofp, PRINTF_TEST_FORMAT, PRINTF_TEST_ARGS) != PRINTF_TEST_LENGTH) {
    error_info_exit(1);
  }
  if (!PRINTF_TEST_CHECK) {
    error_info_exit(1);
  }

  return 0;
}


static int main_test_misc(FILE *ifp, FILE *ofp)
{
  /* misc. */
  if (ferror(ifp)) {
    error_info_exit(1);
  }
  if (ferror(ofp)) {
    error_info_exit(1);
  }

  clearerr(ifp);
  clearerr(ofp);

  if (feof(ifp)) {
    error_info_exit(1);
  }
  if (feof(ofp)) {
    error_info_exit(1);
  }

  if (fileno(ifp) == -1) {
    error_info_exit(1);
  }
  if (fileno(ofp) == -1) {
    error_info_exit(1);
  }

  return 0;
}

int main_test(FILE *ifp, FILE *ofp)
{
  if (main_test_1(ifp, ofp)) {
    error_info_exit(1);
  }

  if (main_test_2(ifp, ofp)) {
    error_info_exit(1);
  }

  if (main_test_misc(ifp, ofp)) {
    error_info_exit(1);
  }

  return 0;
}

int main_seekable_test(FILE *ifp, FILE *ofp)
{
  long pos;
  fpos_t fpos;
  char stdio_buf[BUFSIZ];

  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  /* go to the beginning by rewind */
  rewind(ifp);
  pos = ftell(ifp);
  if (pos != 0) {
    error_info_exit(1);
  }

  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  /* go to the beginning by fseek */
  if (fseek(ifp, 1, SEEK_SET)) {
    error_info_exit(1);
  }

  /* save position */
  if (fgetpos(ifp, &fpos)) {
    error_info_exit(1);
  }

  /* do the same test again */
  if (main_test_2(ifp, ofp)) {
    error_info_exit(1);
  }

  /* restore position */
  if (fsetpos(ifp, &fpos)) {
    error_info_exit(1);
  }

  /* do the same test again */
  if (main_test_2(ifp, ofp)) {
    error_info_exit(1);
  }

  /* EOF */
  if (fseek(ifp, 0, SEEK_END)) {
    error_info_exit(1);
  }
  if (getc(ifp) != EOF) { /* set EOF indicator */
    error_info_exit(1);
  }
  if (!feof(ifp)) {
    error_info_exit(1);
  }
  /* error */
  if (fputc('\n', ifp) != EOF) { /* set error indicator */
    error_info_exit(1);
  }
  if (!ferror(ifp)) {
    error_info_exit(1);
  }

  /* clear error */
  clearerr(ifp);
  if (feof(ifp)) {
    error_info_exit(1);
  }
  if (ferror(ifp)) {
    error_info_exit(1);
  }

  /*** setvbuf, setbuf ***/
  /* _IONBUF */
  rewind(ifp);
  if (setvbuf(ofp, NULL, _IONBF, 0)) {
    error_info_exit(1);
  }
  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  /* _IOLBUF */
  rewind(ifp);
  if (setvbuf(ofp, NULL, _IOLBF, 0)) {
    error_info_exit(1);
  }
  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  /* _IOFBUF */
  rewind(ifp);
  if (setvbuf(ofp, NULL, _IOFBF, 0)) {
    error_info_exit(1);
  }
  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  rewind(ifp);
  if (setvbuf(ofp, stdio_buf, _IOFBF, 0)) {
    error_info_exit(1);
  }
  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  rewind(ifp);
  setbuf(ofp, NULL);
  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  rewind(ifp);
  setbuf(ofp, stdio_buf);
  /* do the same test again */
  if (main_test(ifp, ofp)) {
    error_info_exit(1);
  }

  setbuf(ofp, NULL); /* unset buffer */

  return 0;
}

int main_tmp_test(FILE *tmpfp, FILE *ifp, FILE *ofp)
{
  long size;
  char buf[BUFFER_SIZE];

  if (fseek(ifp, 0, SEEK_END)) {
    error_info_exit(1);
  }

  size = ftell(ifp);

  rewind(ifp);

  if (fread(buf, size, 1, ifp) != 1) {
    error_info_exit(1);
  }

  if (fwrite(buf, size, 1, tmpfp) != 1) {
    error_info_exit(1);
  }

  rewind(tmpfp);

  if (fread(buf, size, 1, tmpfp) != 1) {
    error_info_exit(1);
  }

  if (fwrite(buf, size, 1, ofp) != 1) {
    error_info_exit(1);
  }

  return 0;
}
