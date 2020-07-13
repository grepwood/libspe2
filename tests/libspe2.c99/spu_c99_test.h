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

#ifndef __SPU_C99_TEST_H
#define __SPU_C99_TEST_H

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "spu_libspe2_test.h"

#ifdef STDIO_INTEGER_ONLY
#  define printf    iprintf
#  define fprintf   fiprintf
#  define sprintf   siprintf
#  define snprintf  sniprintf
#  define vprintf   viprintf
#  define vfprintf  vfiprintf
#  define vsprintf  vsiprintf
#  define vsnprintf vsniprintf
#  define scanf     iscanf
#  define fscanf    fiscanf
#  define sscanf    siscanf
#  define vscanf    viscanf
#  define vfscanf   vfiscanf
#  define vsscanf   vsiscanf
#endif

/* The format %n is inhibited on PPE side (glibc) by security
   reason. */
//#define ENABLE_TEST_FORMAT_N

#ifdef ENABLE_TEST_FORMAT_N
#  define FORMAT_N(n) n
#  define FORMAT_N_ARG(n) ,n
#else
#  define FORMAT_N(n)
#  define FORMAT_N_ARG(n)
#endif

#define PRINTF_TEST_TEXT_MAX 256

#define PRINTF_TEST_VARS_DECL						\
  int i0, i1, i2, i3, i4, i5; short h1;					\
  signed char c1, c2; long l0, l1, l2, l3; long long L0, L1;		\
  size_t z1; intmax_t j1; ptrdiff_t t1;					\
  wchar_t w1;								\
  float f1, f2, f3; double d1, d2, d3;					\
  char s1[PRINTF_TEST_TEXT_MAX], s2[PRINTF_TEST_TEXT_MAX];		\
  wchar_t ws1[PRINTF_TEST_TEXT_MAX], ws2[PRINTF_TEST_TEXT_MAX];		\
  void *p1

#define PRINTF_TEST_TEXT "The_quick_brown_fox_jumps_over_the_lazy_dog."
#define PRINTF_TEST_TEXT_LENGTH 44

#define SCANF_TEST_NUM        25
#define SCANF_TEST_FORMAT						\
  "%%u\n"   /* ignored */						\
  "%*10d\n" /* ignored */						\
  "%u\n"								\
  FORMAT_N("%n")     /* # of consumed chars */				\
  "%d\n"								\
  FORMAT_N("%ln")    /* # of consumed chars */				\
  "%x\n"								\
  FORMAT_N("%lln")   /* # of consumed chars */				\
  "%i\n"								\
  "%hd\n"								\
  "%hhd\n"								\
  "%c\n"								\
  "%lc\n"								\
  "%ld\n"								\
  "%lld\n"								\
  "%zu\n"								\
  "%jd\n"								\
  "%td\n"								\
  "%08lx\n"								\
  "%f\n"								\
  "%e\n"								\
  "%a\n"								\
  "%lf\n"								\
  "%le\n"								\
  "%la\n"								\
  "%s\n"								\
  "%ls\n"								\
  "%[]ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.[]\n"	\
  "%l[]ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.[]\n"	\
  "%p\n"
#define SCANF_TEST_ARGS					\
  &i1 FORMAT_N_ARG(&i0), &i2 FORMAT_N_ARG(&l0), &i3 FORMAT_N_ARG(&L0), &i4, \
    &h1, &c1, &c2, &w1, &l1, &L1, &z1, &j1, &t1, &l2,	\
    &f1, &f2, &f3, &d1, &d2, &d3, s1, ws1, s2, ws2, &p1
#define SCANF_TEST_INIT_VARS		\
  (					\
   i0 = i1 = i2 = i3 = i4 = i5 = -1,	\
   h1 = -1, c1 = c2 = -1,		\
   l0 = l1 = l2 = l3 = -1,		\
   L0 = L1 = -1,			\
   z1 = -1, j1 = -1, t1 = -1,		\
   w1 = -1,				\
   f1 = f2 = f3 = -1.0f,		\
   d1 = d2 = d3 = -1.0,			\
   memset(s1, -1, sizeof(s1)),		\
   memset(s2, -1, sizeof(s2)),		\
   memset(ws1, -1, sizeof(ws1)),	\
   memset(ws2, -1, sizeof(ws2)),	\
   p1 = (void*)-1)
#define SCANF_TEST_STRING			\
  "%u\n"		/* ignored */		\
  "0123456789\n"	/* ignored */		\
  "123456789\n"					\
  "-987654321\n"				\
  "fedcba98\n"					\
  "-123456789\n"				\
  "24680\n"					\
  "-123\n"					\
  "@\n"						\
  "*\n"						\
  "-987654321\n"				\
  "123456789012\n"				\
  "987654321\n"					\
  "210987654321\n"				\
  "-123456789\n"				\
  "09abcdef\n"					\
  "1.0\n"					\
  "-1.0e+10\n"					\
  "0x1.cp+1\n"					\
  "2.0\n"					\
  "-1.0e+11\n"					\
  "0x1.cp+2\n"					\
  PRINTF_TEST_TEXT "\n"				\
  PRINTF_TEST_TEXT "\n"				\
  PRINTF_TEST_TEXT "\n"				\
  PRINTF_TEST_TEXT "\n"				\
  "0xfedcba98\n"
#define SCANF_TEST_CHECK	\
  (				\
   i1 == 123456789 &&		\
   FORMAT_N(i0 == 24 &&)	\
   i2 == -987654321 &&		\
   FORMAT_N(l0 == 35 &&)	\
   i3 == 0xfedcba98 &&		\
   FORMAT_N(L0 == 44 &&)	\
   i4 == -123456789 &&		\
   c1 == -123 &&		\
   c2 == '@' &&			\
   w1 == L'*' &&		\
   l1 == -987654321L &&		\
   L1 == 123456789012LL &&	\
   z1 == 987654321 &&		\
   j1 == 210987654321LL &&	\
   t1 == -123456789 &&		\
   l2 == 0x09abcdef &&		\
   f1 == 1.0f &&		\
   f2 == -1.0e+10f &&		\
   f3 == 0x1.cp+1f &&		\
   d1 == 2.0 &&			\
   d2 == -1.0e+11 &&		\
   d3 == 0x1.cp+2 &&		\
   memcmp(s1, PRINTF_TEST_TEXT, sizeof(s1[0]) * (PRINTF_TEST_TEXT_LENGTH + 1)) == 0 && \
   memcmp(s2, PRINTF_TEST_TEXT, sizeof(s2[0]) * (PRINTF_TEST_TEXT_LENGTH + 1)) == 0 && \
   memcmp(ws1, WCHAR(PRINTF_TEST_TEXT), sizeof(ws1[0]) * (PRINTF_TEST_TEXT_LENGTH + 1)) == 0 && \
   memcmp(ws2, WCHAR(PRINTF_TEST_TEXT), sizeof(ws2[0]) * (PRINTF_TEST_TEXT_LENGTH + 1)) == 0 && \
   p1 == (void *)0xfedcba98 )

#define PRINTF_TEST_FORMAT			\
  "%%u\n" /* '%' itself */			\
  "%u\n"					\
  FORMAT_N("%n")	/* # of output chars */	\
  "%d\n"					\
  FORMAT_N("%ln")				\
  "%x\n"					\
  FORMAT_N("%lln")				\
  "%i\n"					\
  "%hd\n"					\
  "%hhd\n"					\
  "%c\n"					\
  "%lc\n"					\
  "%ld\n"					\
  "%lld\n"					\
  "%zu\n"					\
  "%jd\n"					\
  "%td\n"					\
  "%08lx\n"					\
  "%+10ld\n"					\
  "%.1f\n"					\
  "%.1e\n"					\
  "%a\n"					\
  "%s\n"					\
  "%ls\n"					\
  "%p\n"
#define PRINTF_TEST_ARGS			\
  i1 FORMAT_N_ARG(&i0), i2 FORMAT_N_ARG(&l0), i3 FORMAT_N_ARG(&L0), i4, h1, c1, i5, w1, l1, L1, \
    z1, j1, t1, l2, l3, d1, d2, d3, s1, ws1, p1
#define PRINTF_TEST_INIT_VARS						\
  (									\
   i1 = 123456789,							\
   i0 = -1,								\
   i2 = -987654321,							\
   l0 = -1,								\
   i3 = 0xfedcba98,							\
   L0 = -1,								\
   i4 = -123456789,							\
   h1 = 24680,								\
   c1 = -123,								\
   i5 = '@',								\
   w1 = L'*',								\
   l1 = -987654321,							\
   L1 = 123456789012LL,							\
   z1 = 987654321,							\
   j1 = 210987654321LL,							\
   t1 = -123456789,							\
   l2 = 0x09abcdef,							\
   l3 = 1234567,							\
   f1 = 1.0f,								\
   f2 = -1.0e+10f,							\
   f3 = 0x1.cp+1f,							\
   d1 = 2.0,								\
   d2 = -1.0e+11,							\
   d3 = 0x1.cp+2,							\
   p1 = (void *)0xfedcba98,						\
   memcpy(s1, PRINTF_TEST_TEXT, sizeof(s1[0]) * (PRINTF_TEST_TEXT_LENGTH + 1)), \
   memcpy(ws1, WCHAR(PRINTF_TEST_TEXT), sizeof(ws1[0]) * (PRINTF_TEST_TEXT_LENGTH + 1)) )
#define PRINTF_TEST_STRING	\
  "%u\n"			\
  "123456789\n"			\
  "-987654321\n"		\
  "fedcba98\n"			\
  "-123456789\n"		\
  "24680\n"			\
  "-123\n"			\
  "@\n"				\
  "*\n"				\
  "-987654321\n"		\
  "123456789012\n"		\
  "987654321\n"			\
  "210987654321\n"		\
  "-123456789\n"		\
  "09abcdef\n"			\
  "  +1234567\n"		\
  "2.0\n"			\
  "-1.0e+11\n"			\
  "0x1.cp+2\n"			\
  PRINTF_TEST_TEXT "\n"		\
  PRINTF_TEST_TEXT "\n"		\
  "0xfedcba98\n"
#define PRINTF_TEST_LENGTH    strlen(PRINTF_TEST_STRING)
#define PRINTF_TEST_CHECK \
  ( FORMAT_N(i0 == 13 &&) FORMAT_N(l0 == 24 &&) FORMAT_N(L0 == 33 &&) 1 )

#endif /* __SPU_C99_TEST_H */
