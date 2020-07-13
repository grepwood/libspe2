/* default_c99_handler.c - emulate SPE C99 library calls.
 *
 * Copyright (C) 2005 IBM Corp.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <pthread.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <linux/limits.h>
#include <bits/posix2_lim.h>

#include "default_c99_handler.h"
#include "handler_utils.h"
#include "spebase.h"

/* SPE C99 Handlers - Overview:
 * This file implements handlers for SPE C99 library operations such 
 * as printf(3) using standard C library facilities that are available 
 * on the PPE.
 *
 * This approach optimizes for space over time.  The local store footprint
 * of the SPE C99 library is reduced by handling file and string operations
 * on the PPE.  This allows the programmer to reserve local store for high
 * compute intensity software, while still providing functional interfaces.
 *
 * The major drawback of this approach is performance.  Since I/O is not
 * buffered on the SPE, application performance may suffer with repeated
 * calls to putc(3), for instance.  Applications requiring high performance
 * buffered I/O should consider other solutions, such as NEWLIB.
 *
 * SPE-side Stubs:
 * The SPE-side of this interface is most likely implemented with a set of
 * assembly language stub routines.  These are responsible for pushing SPE
 * register parameters onto the stack per the SPE-ABI, and then for executing
 * a stop-and-signal instruction with a reserved signal code 
 * (currently 0x2100/SPE_PROGRAM_LIBRARY_CALL). 
 * By convention, the next word following the stop-and-signal instruction
 * will contain the C99 op-code along with a pointer to the beginning of
 * the call stack frame.
 *
 * Variable argument library calls are converted side into equivilent va_list
 * form, i.e. printf(3) becomes vprintf(3) and scanf(3) becomes vscanf(3).  This * conversion takes place within the SPE stub routines.
 *
 * PPE-Side Handlers:
 * The PPE application or library receives stop-and-signal notification
 * from the OS.  The default_c99_handler() routine is then called in order
 * to parse the C99 op-code and branch to a specific C99 handler, such as
 * default_c99_handler_vprintf().
 *
 * The C99 handlers use direct-mapped access to LS in order to fetch parameters
 * from the stack frame, and to place return values (including errno) into the
 * expected locations.
 */

#define SPE_C99_OP_SHIFT    	24
#define SPE_C99_OP_MASK	    	0xff
#define SPE_C99_DATA_MASK   	0xffffff
#define SPE_C99_OP(_v) 		(((_v) >> SPE_C99_OP_SHIFT) & SPE_C99_OP_MASK)
#define SPE_C99_DATA(_v) 	((_v) & SPE_C99_DATA_MASK)

enum { 
	SPE_C99_UNUSED,
	SPE_C99_CLEARERR,
	SPE_C99_FCLOSE,
	SPE_C99_FEOF,
	SPE_C99_FERROR,
	SPE_C99_FFLUSH,
	SPE_C99_FGETC,
	SPE_C99_FGETPOS,
	SPE_C99_FGETS,
	SPE_C99_FILENO,
	SPE_C99_FOPEN,
	SPE_C99_FPUTC,
	SPE_C99_FPUTS,
	SPE_C99_FREAD,
	SPE_C99_FREOPEN,
	SPE_C99_FSEEK,
	SPE_C99_FSETPOS,
	SPE_C99_FTELL,
	SPE_C99_FWRITE,
	SPE_C99_GETC,
	SPE_C99_GETCHAR,
	SPE_C99_GETS,
	SPE_C99_PERROR,
	SPE_C99_PUTC,
	SPE_C99_PUTCHAR,
	SPE_C99_PUTS,
	SPE_C99_REMOVE,
	SPE_C99_RENAME,
	SPE_C99_REWIND,
	SPE_C99_SETBUF,
	SPE_C99_SETVBUF,
	SPE_C99_SYSTEM,
	SPE_C99_TMPFILE,
	SPE_C99_TMPNAM,
	SPE_C99_UNGETC,
	SPE_C99_VFPRINTF,
	SPE_C99_VFSCANF,
	SPE_C99_VPRINTF,
	SPE_C99_VSCANF,
	SPE_C99_VSNPRINTF,
	SPE_C99_VSPRINTF,
	SPE_C99_VSSCANF,
	SPE_C99_LAST_OPCODE,
};
#define SPE_C99_NR_OPCODES 	((SPE_C99_LAST_OPCODE - SPE_C99_CLEARERR) + 1)

#define SPE_STDIN                   1
#define SPE_STDOUT                  2
#define SPE_STDERR                  3
#define SPE_FOPEN_MAX               (FOPEN_MAX+1)
#define SPE_FOPEN_MIN               4

#define SPE_STDIO_BUFSIZ            1024

/**
 * spe_FILE_ptrs - an indexed array of 'FILE *', used by SPE C99 calls.
 *
 * A layer of indirection to report back indices rather than 'FILE *',
 * so as to be type safe w/r/t the 64-bit PPC-ABI.
 *
 * The indices {0,1,2,3} are aliases to {NULL,stdin,stdout,stderr}.
 */
static pthread_mutex_t spe_c99_file_mutex = PTHREAD_MUTEX_INITIALIZER;
static int nr_spe_FILE_ptrs = 3;
static FILE *spe_FILE_ptrs[SPE_FOPEN_MAX] = {
    NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
};

typedef unsigned long long __va_elem;

/* Allocate stack space for vargs array. */
#define __VA_LIST_ALIGN 16UL

#define __VA_LIST_ALLOCA(_nr)    \
       (__va_elem *)(((unsigned long)alloca((_nr+1) * sizeof(__va_elem) + __VA_LIST_ALIGN) \
                      + __VA_LIST_ALIGN - 1) & ~(__VA_LIST_ALIGN - 1))

#ifdef __powerpc64__
#define __VA_LIST_PUT(_vargs, _type, _a)                               \
        ((_type *) ((unsigned long) _vargs + sizeof(__va_elem) - sizeof(_type)))[0] = (_a); \
        _vargs = (__va_elem *)_vargs + 1

#define __VA_TEMP_ALLOCA(_nr) (struct __va_temp *) alloca((_nr+1) * sizeof(struct __va_temp))

#else /* !__powerpc64__ */

#define __OFFSET(_type)	(sizeof(_type)-1)
#define __MASK(_type)	~(__OFFSET(_type))

/* Align '_vargs' properly for '_type'. */
#define __VA_LIST_ALIGN_TO_TYPE(_vargs, _type)			\
    (((unsigned long) (_vargs) + __OFFSET(_type)) & __MASK(_type))

/* Put a value into the va_list pointed to by '_vargs'. 
 */
#define __VA_LIST_PUT(_vargs, _type, _a)                		\
	_vargs = (__va_elem *) __VA_LIST_ALIGN_TO_TYPE(_vargs, _type);	\
        ((_type *) _vargs)[0] = (_a);                   		\
        _vargs = (__va_elem *)(((_type *) _vargs) + 1)

#define __VA_TEMP_ALLOCA(_nr) NULL /* unused */

#endif /* !__powerpc64__ */

#define GET_LS_VARG(_name) {                        			  \
        memcpy(&(_name), GET_LS_PTR(spe_vlist->next_arg), sizeof(_name)); \
        spe_vlist->next_arg += 16;					  \
	if ((spe_vlist->next_arg & LS_ADDR_MASK) ==                       \
            (spe_vlist->caller_stack & LS_ADDR_MASK)) {		          \
		spe_vlist->next_arg += 32;				  \
	}								  \
}

/* SPE va_list structure, 
 * per SPU-ABI definition.
 */
struct spe_va_list {
	unsigned int next_arg;
	unsigned int pad0[3];
	unsigned int caller_stack;
	unsigned int pad1[3];
};

#if !defined(__powerpc64__)
/* PPC-32 va_list structure. 
 * When nr is greater than 8,
 * called function takes parms 
 * from 'ptr'.
 */
struct __ppc32_va_list {
    unsigned char nr_gpr;
    unsigned char nr_fpr;
    unsigned short reserved;
    void *ptr;
};
#endif /* __powerpc64__ */

/* Temporary area to save long value or pointer on ppc64. */
struct __va_temp {
  long long llval;
  int *ptr;
};

static int __do_vfprintf(FILE * stream, char *format, __va_elem * vlist)
{
#if !defined(__powerpc64__)
    struct __ppc32_va_list ap;

    ap.nr_gpr = 127;
    ap.nr_fpr = 127;
    ap.ptr = (void *) vlist;
    return vfprintf(stream, format, (void *) &ap);
#else
    return vfprintf(stream, format, (void *) vlist);
#endif 
}

static int __do_vsprintf(char *string, char *format, __va_elem * vlist)
{
#if !defined(__powerpc64__)
    struct __ppc32_va_list ap;

    ap.nr_gpr = 127;
    ap.nr_fpr = 127;
    ap.ptr = (void *) vlist;
    return vsprintf(string, format, (void *) &ap);
#else
    return vsprintf(string, format, (void *) vlist);
#endif 
}

static int __do_vsnprintf(char *string, size_t size, char *format, 
                          __va_elem * vlist)
{
#if !defined(__powerpc64__)
    struct __ppc32_va_list ap;

    ap.nr_gpr = 127;
    ap.nr_fpr = 127;
    ap.ptr = (void *) vlist;
    return vsnprintf(string, size, format, (void *) &ap);
#else
    return vsnprintf(string, size, format, (void *) vlist);
#endif 
}

static int __do_vfscanf(FILE * stream, char *format, __va_elem * vlist)
{
#if !defined(__powerpc64__)
    struct __ppc32_va_list ap;

    ap.nr_gpr = 127;
    ap.nr_fpr = 127;
    ap.ptr = (void *) vlist;
    return vfscanf(stream, format, (void *) &ap);
#else
    return vfscanf(stream, format, (void *) vlist);
#endif 
}

static int __do_vsscanf(char *string, char *format, __va_elem * vlist)
{
#if !defined(__powerpc64__)
    struct __ppc32_va_list ap;

    ap.nr_gpr = 127;
    ap.nr_fpr = 127;
    ap.ptr = (void *) vlist;
    return vsscanf(string, format, (void *) &ap);
#else
    return vsscanf(string, format, (void *) vlist);
#endif 
}

#ifdef __powerpc64__
static void __copy_va_temp(struct __va_temp *vtemps)
{
    while (vtemps->ptr) {
      *vtemps->ptr = vtemps->llval;
      vtemps++;
    }
}
#else /* !__powerpc64__ */
#define __copy_va_temp(vtemps) /* do nothing */
#endif /* __powerpc64__ */

static inline FILE *get_FILE_nolock(int nr)
{
    FILE *ret;

    if (nr <= 0) {
	ret = NULL;
    } else if (nr >= SPE_FOPEN_MAX) {
	ret = NULL;
    } else {
	switch (nr) {
	case SPE_STDIN:
	    ret = (spe_FILE_ptrs[1]) ? spe_FILE_ptrs[1] : stdin;
	    break;
	case SPE_STDOUT:
	    ret = (spe_FILE_ptrs[2]) ? spe_FILE_ptrs[2] : stdout;
	    break;
	case SPE_STDERR:
	    ret = (spe_FILE_ptrs[3]) ? spe_FILE_ptrs[3] : stderr;
	    break;
	default:
	    ret = spe_FILE_ptrs[nr];
	    break;
	}
    }
    return ret;
}

static inline FILE *get_FILE(int nr)
{
    FILE *ret;

    pthread_mutex_lock(&spe_c99_file_mutex);
    ret = get_FILE_nolock(nr);
    pthread_mutex_unlock(&spe_c99_file_mutex);
    return ret;
}

#define SKIP_PRECISION(p, pr) {         \
  pr = 0;                               \
  if (*p == '.') {                      \
    switch (*++p) {                     \
    case '0':                           \
    case '1':                           \
    case '2':                           \
    case '3':                           \
    case '4':                           \
    case '5':                           \
    case '6':                           \
    case '7':                           \
    case '8':                           \
    case '9':                           \
      while (*p && isdigit(*p)) p++;	\
      break;                            \
    case '*':                           \
      GET_LS_VARG(pr);                  \
      __VA_LIST_PUT(vlist, int, pr);    \
      p++;                              \
      break;                            \
    default:                            \
      break;                            \
    }                                   \
  }                                     \
}

#define SKIP_FIELD_WIDTH(p, fw, output) { \
  fw = 0;                               \
  switch (*p) {                         \
  case '0':                             \
  case '1':                             \
  case '2':                             \
  case '3':                             \
  case '4':                             \
  case '5':                             \
  case '6':                             \
  case '7':                             \
  case '8':                             \
  case '9':                             \
    while (*p && isdigit(*p)) p++;	\
    break;                              \
  case '*':                             \
    if (output) {                       \
      GET_LS_VARG(fw);                  \
      __VA_LIST_PUT(vlist, int, fw);    \
    }                                   \
    p++;                                \
    break;                              \
  default:                              \
    break;                              \
  }                                     \
}

#define SKIP_PRINTF_FLAG_CHARS(p) {     \
  int done = 0;                         \
  do {                                  \
    switch (*p) {                       \
    case '#':                           \
    case '0':                           \
    case '-':                           \
    case '+':                           \
    case ' ':                           \
      p++;                              \
      break;                            \
    default:                            \
      done = 1;                         \
      break;                            \
    }                                   \
  } while (!done);                      \
}

#define SKIP_LENGTH_MODIFIERS(p, h, l) {        \
  int done = 0;                                 \
  h = 0; l = 0;                                 \
  do {                                          \
    switch (*p) {                               \
    case 'h':                                   \
      h++;                                      \
      p++;                                      \
      break;                                    \
    case 'l':                                   \
      l++;                                      \
      p++;                                      \
      break;                                    \
    case 'j':                                   \
      l = 2;                                    \
      p++;                                      \
      break;                                    \
    case 'z':                                   \
    case 't':                                   \
      l = 1;                                    \
      p++;                                      \
      break;                                    \
    default:                                    \
      done = 1;                                 \
      break;                                    \
    }                                           \
  } while (!done);                              \
}

#define COUNT_FIELD_WIDTH(p, output) {  \
  switch (*p) {                         \
  case '0':                             \
  case '1':                             \
  case '2':                             \
  case '3':                             \
  case '4':                             \
  case '5':                             \
  case '6':                             \
  case '7':                             \
  case '8':                             \
  case '9':                             \
    while (*p && isdigit(*p)) p++;	\
    break;                              \
  case '*':                             \
    p++;                                \
    if (output) nr++;                   \
    break;                              \
  default:                              \
    break;                              \
  }                                     \
}

#define COUNT_PRECISION(p, output) {    \
  if (*p == '.') {                      \
    switch (*++p) {                     \
    case '0':                           \
    case '1':                           \
    case '2':                           \
    case '3':                           \
    case '4':                           \
    case '5':                           \
    case '6':                           \
    case '7':                           \
    case '8':                           \
    case '9':                           \
      while (*p && isdigit(*p)) p++;	\
      break;                            \
    case '*':                           \
      if (output) nr++;                 \
      p++;                              \
      break;                            \
    default:                            \
      break;                            \
    }                                   \
  }                                     \
}

#define SKIP_SCANF_FLAG_CHARS(p, sp) {          \
  int done = 0;                                 \
  sp = 0;                                       \
  do {                                          \
    switch (*p) {                               \
    case '*':                                   \
      sp = 1;                                   \
      p++;                                      \
      break;                                    \
    default:                                    \
      done = 1;                                 \
      break;                                    \
    }                                           \
  } while (!done);                              \
}

#define SKIP_CHAR_SET(p)                        \
{                                               \
  p++;                                          \
  if (*p == '^') p++;                           \
  if (*p == ']') p++;                           \
  p = strchr(p, ']');                           \
  if (p == NULL) {                              \
    DEBUG_PRINTF("SKIP_CHAR_SET() error.");     \
    return 1;                                   \
  }                                             \
}

static int __nr_format_args(const char *format)
{
    char *p;
    int nr = 0;

    for (p = (char *) format; *p; p++, nr++) {
        p = strchr(p, '%');
        if (!p) {
            /* Done with formatting. */
            break;
        }
    }
    /* Loosely estimate of the number of format arguments
     * by scanning for '%'.  Multiply the return value by 
     * 3 in order to account for variable format width or
     * precision.
     */
    return nr * 3;
}

static int __parse_printf_format(char *ls, char *format,
			         struct spe_va_list *spe_vlist,
			         __va_elem * vlist, struct __va_temp *vtemps, int nr_vargs)
{
    int fw, pr;
    int format_half, format_long;
    int ival;
    double dval;
    long long llval;
    unsigned int ls_offset;
    char *p;
    void *ptr;

    for (p = format; nr_vargs > 0 && *p; p++) {
	p = strchr(p, '%');
	if (!p) {
	    /* Done with formatting. */
	    break;
	}
	p++;
	nr_vargs--;
	SKIP_PRINTF_FLAG_CHARS(p);
	SKIP_FIELD_WIDTH(p, fw, 1);
	SKIP_PRECISION(p, pr);
	SKIP_LENGTH_MODIFIERS(p, format_half, format_long);
	switch (*p) {
	case 'd':
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
	    if (format_long == 2) {
		GET_LS_VARG(llval);
		__VA_LIST_PUT(vlist, long long, llval);
		break;
#ifdef __powerpc64__
	    } else if (format_long) {
		GET_LS_VARG(ival);
		switch (*p) {
		case 'd':
		case 'i':
		    __VA_LIST_PUT(vlist, long, (long)ival);
		    break;
		default:
		    __VA_LIST_PUT(vlist, unsigned long,
				  (unsigned long)(unsigned int)ival);
		    break;
		}
		break;
#endif /* __powerpc64__*/
	    }
	    /* fall through */
	case 'c':
	    GET_LS_VARG(ival);
	    __VA_LIST_PUT(vlist, int, ival);
	    break;
	case 'a':
	case 'A':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	    GET_LS_VARG(dval);
	    __VA_LIST_PUT(vlist, double, dval);
	    break;
	case 'p':
	    GET_LS_VARG(ival);
	    __VA_LIST_PUT(vlist, unsigned long,
			  (unsigned long)(unsigned int)ival);
	    break;
	case 's':
	    GET_LS_VARG(ls_offset);
	    ptr = GET_LS_PTR(ls_offset);
	    __VA_LIST_PUT(vlist, char *, ptr);
	    break;
	case 'n':
	    GET_LS_VARG(ls_offset);
	    ptr = GET_LS_PTR(ls_offset);
#ifdef __powerpc64__
	    if (format_long == 1) {
		vtemps->ptr = ptr;
		__VA_LIST_PUT(vlist, long long *, &vtemps->llval);
		vtemps++;
		break;
	    }
#endif /* __powerpc64__ */
	    __VA_LIST_PUT(vlist, void *, ptr);
	    break;
	default:
	    break;
	}
    }
#ifdef __powerpc64__
    vtemps->ptr = NULL;
#endif /* __powerpc64__ */
    return 0;
}

static int __parse_scanf_format(char *ls, char *format,
			        struct spe_va_list *spe_vlist,
                               __va_elem * vlist, struct __va_temp *vtemps, int nr_vargs)
{
    int fw;
    int format_half, format_long, suppress;
    unsigned int ls_offset;
    char *p;
    void *ptr;

    for (p = format; nr_vargs > 0 && *p; p++) {
	p = strchr(p, '%');
	if (!p) {
	    /* No more formatting. */
	    break;
	}
	p++;
	nr_vargs--;
	SKIP_SCANF_FLAG_CHARS(p, suppress);
	SKIP_FIELD_WIDTH(p, fw, 0);
	SKIP_LENGTH_MODIFIERS(p, format_half, format_long);
	switch (*p) {
	case 'd':
	case 'i':
	case 'o':
	case 'u':
	case 'x':
	case 'X':
	case 'n':
#ifdef __powerpc64__
           if (format_long == 1) {
               if (!suppress) {
                   GET_LS_VARG(ls_offset);
                   ptr = GET_LS_PTR(ls_offset);
                   vtemps->ptr = ptr;
                   __VA_LIST_PUT(vlist, long long *, &vtemps->llval);
                   vtemps++;
               }
	       break;
	   }
#endif /* __powerpc64__ */
	   /* fall through */
	case 'a':
	case 'A':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
	case 'g':
	case 'G':
	case 'c':
	case 's':
	    if (!suppress) {
		GET_LS_VARG(ls_offset);
		ptr = GET_LS_PTR(ls_offset);
		__VA_LIST_PUT(vlist, void *, ptr);
	    }
	    break;
	case 'p':
	    if (!suppress) {
#ifdef __powerpc64__
		GET_LS_VARG(ls_offset);
		ptr = GET_LS_PTR(ls_offset);
		vtemps->ptr = ptr;
		__VA_LIST_PUT(vlist, long long *, &vtemps->llval);
		vtemps++;
#else /* !__powerpc64__ */
		GET_LS_VARG(ls_offset);
		ptr = GET_LS_PTR(ls_offset);
		__VA_LIST_PUT(vlist, int *, ptr);
#endif /* !__powerpc64__ */
	    }
	    break;
	case '[':
	    SKIP_CHAR_SET(p);
	    if (!suppress) {
		GET_LS_VARG(ls_offset);
		ptr = GET_LS_PTR(ls_offset);
		__VA_LIST_PUT(vlist, char *, ptr);
	    }
	    break;
	case '%':
	    break;
	default:
	    break;
	}
    }
#ifdef __powerpc64__
    vtemps->ptr = NULL;
#endif /* __powerpc64__ */
    return 0;
}

/**
 * default_c99_handler_remove
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int remove(const char *pathname);
 */
static int default_c99_handler_remove(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *path;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    rc = remove(path);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_rename
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int rename(const char *oldname, const char *newname);
 */
static int default_c99_handler_rename(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *oldname;
    char *newname;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    oldname = GET_LS_PTR(arg0->slot[0]);
    newname = GET_LS_PTR(arg1->slot[0]);
    rc = rename(oldname, newname);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_tmpfile
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	FILE *tmpfile(void);
 */
static int default_c99_handler_tmpfile(char *ls, unsigned long opdata)
{
    DECL_0_ARGS();
    DECL_RET();
    int i;

    DEBUG_PRINTF("%s\n", __func__);
    if (nr_spe_FILE_ptrs >= SPE_FOPEN_MAX) {
	PUT_LS_RC(0, 0, 0, EMFILE);
    } else {
	for (i = SPE_FOPEN_MIN; i < SPE_FOPEN_MAX; i++) {
	    if (spe_FILE_ptrs[i] == NULL) {
		spe_FILE_ptrs[i] = tmpfile();
		if (spe_FILE_ptrs[i])
		    nr_spe_FILE_ptrs++;
		else
		    i = 0;
		PUT_LS_RC(i, 0, 0, errno);
		break;
	    }
	}
	if (i == SPE_FOPEN_MAX) {
	    PUT_LS_RC(0, 0, 0, EMFILE);
	}
    }
    return 0;
}

/**
 * default_c99_handler_tmpnam
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	char *tmpnam(char *s);
 * 
 * For integrity reasons we return failure. We should expose 
 * mkstemp() instead.
 */
static int default_c99_handler_tmpnam(char *ls, unsigned long opdata)
{
    DECL_0_ARGS();
    DECL_RET();

    DEBUG_PRINTF("%s\n", __func__);
    PUT_LS_RC(0, 0, 0, 0);
    return 0;
}

/**
 * default_c99_handler_fclose
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fclose(FILE *stream);
 */
static int default_c99_handler_fclose(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    pthread_mutex_lock(&spe_c99_file_mutex);
    rc = fclose(stream);
    if (rc == 0) {
	spe_FILE_ptrs[arg0->slot[0]] = NULL;
	nr_spe_FILE_ptrs--;
    }
    pthread_mutex_unlock(&spe_c99_file_mutex);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fflush
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fflush(FILE *stream);
 */
static int default_c99_handler_fflush(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = fflush(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fopen
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	FILE *fopen(const char *path, const char *mode);
 */
static int default_c99_handler_fopen(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *path;
    char *mode;
    FILE *f;
    int i, rc = 0, err = EMFILE;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR_NULL(arg0->slot[0]);
    mode = GET_LS_PTR_NULL(arg1->slot[0]);
    pthread_mutex_lock(&spe_c99_file_mutex);
    if (nr_spe_FILE_ptrs < SPE_FOPEN_MAX) {
	for (i = SPE_FOPEN_MIN; i < SPE_FOPEN_MAX; i++) {
	    if (spe_FILE_ptrs[i] == NULL) {
		f = fopen(path, mode);
		err = errno;
		if (f) {
		    spe_FILE_ptrs[i] = f;
		    nr_spe_FILE_ptrs++;
		    rc = i;
		}
		break;
	    }
	}
    }
    pthread_mutex_unlock(&spe_c99_file_mutex);
    PUT_LS_RC(rc, 0, 0, err);
    return 0;
}

/**
 * default_c99_handler_freopen
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	FILE *freopen(const char *path, const char *mode, FILE *stream);
 */
static int default_c99_handler_freopen(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *path;
    char *mode;
    int i;

    DEBUG_PRINTF("%s\n", __func__);
    i = arg2->slot[0];
    if ((i <= 0) || (i >= SPE_FOPEN_MAX)) {
	PUT_LS_RC(1, 0, 0, EBADF);
    } else {
	path = GET_LS_PTR_NULL(arg0->slot[0]);
	mode = GET_LS_PTR_NULL(arg1->slot[0]);
	pthread_mutex_lock(&spe_c99_file_mutex);
	spe_FILE_ptrs[i] = freopen(path, mode, get_FILE_nolock(i));
	if (spe_FILE_ptrs[i]) {
	    PUT_LS_RC(i, 0, 0, 0);
	} else {
	    PUT_LS_RC(0, 0, 0, errno);
	}
	pthread_mutex_unlock(&spe_c99_file_mutex);
    }
    return 0;
}

/**
 * default_c99_handler_setbuf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	void setbuf(FILE *stream, char *buf);
 */
static int default_c99_handler_setbuf(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    FILE *stream;
    char *buf;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    buf = GET_LS_PTR_NULL(arg1->slot[0]);
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, SPE_STDIO_BUFSIZ);
    return 0;
}

/**
 * default_c99_handler_setvbuf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int setvbuf(FILE *stream, char *buf, int mode , size_t size);
 */
static int default_c99_handler_setvbuf(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    FILE *stream;
    char *buf;
    int mode;
    size_t size;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    buf = GET_LS_PTR_NULL(arg1->slot[0]);
    mode = arg2->slot[0];
    size = arg3->slot[0];
    rc = setvbuf(stream, buf, mode, size);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vfprintf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vfprintf(FILE *stream, const char *format, va_list ap);
 */
static int default_c99_handler_vfprintf(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    FILE *stream;
    char *format;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps; /* for %n in 64-bit PPC-ABI */

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    format = GET_LS_PTR(arg1->slot[0]);
    memcpy(&spe_vlist, arg2, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_printf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vfprintf(stream, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = -1;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vfscanf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vfscanf(FILE *stream, const char *format, va_list ap);
 */
static int default_c99_handler_vfscanf(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    FILE *stream;
    char *format;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    format = GET_LS_PTR(arg1->slot[0]);
    memcpy(&spe_vlist, arg2, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_scanf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vfscanf(stream, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = EOF;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vprintf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vprintf(const char *format, va_list ap);
 */
static int default_c99_handler_vprintf(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *stream;
    char *format;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps; /* for %n in 64-bit PPC-ABI */

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(SPE_STDOUT);
    format = GET_LS_PTR(arg0->slot[0]);
    memcpy(&spe_vlist, arg1, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_printf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vfprintf(stream, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = -1;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vscanf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vscanf(const char *format, va_list ap);
 */
static int default_c99_handler_vscanf(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *stream;
    char *format;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(SPE_STDIN);
    format = GET_LS_PTR(arg0->slot[0]);
    memcpy(&spe_vlist, arg1, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_scanf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vfscanf(stream, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = EOF;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vsnprintf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vsnprintf(char *str, size_t size, const char *format, va_list ap);
 */
static int default_c99_handler_vsnprintf(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    char *str;
    char *format;
    size_t size;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps; /* for %n in 64-bit PPC-ABI */

    DEBUG_PRINTF("%s\n", __func__);
    str = GET_LS_PTR(arg0->slot[0]);
    size = arg1->slot[0];
    format = GET_LS_PTR(arg2->slot[0]);
    memcpy(&spe_vlist, arg3, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_printf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vsnprintf(str, size, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = -1;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vsprintf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vsprintf(char *str, const char *format, va_list ap);
 */
static int default_c99_handler_vsprintf(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *str;
    char *format;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps; /* for %n in 64-bit PPC-ABI */

    DEBUG_PRINTF("%s\n", __func__);
    str = GET_LS_PTR(arg0->slot[0]);
    format = GET_LS_PTR(arg1->slot[0]);
    memcpy(&spe_vlist, arg2, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_printf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vsprintf(str, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = -1;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_vsscanf
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int vsscanf(const char *str, const char *format, va_list ap);
 */
static int default_c99_handler_vsscanf(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *str;
    char *format;
    int rc, nr_vargs;
    struct spe_va_list spe_vlist;
    __va_elem *vlist;
    struct __va_temp *vtemps;

    DEBUG_PRINTF("%s\n", __func__);
    str = GET_LS_PTR(arg0->slot[0]);
    format = GET_LS_PTR(arg1->slot[0]);
    memcpy(&spe_vlist, arg2, sizeof(struct spe_va_list));
    nr_vargs = __nr_format_args(format);
    vlist = __VA_LIST_ALLOCA(nr_vargs);
    vtemps = __VA_TEMP_ALLOCA(nr_vargs);
    rc = __parse_scanf_format(ls, format, &spe_vlist, vlist, vtemps, nr_vargs);
    if (rc == 0) {
      rc = __do_vsscanf(str, format, vlist);
      __copy_va_temp(vtemps);
    }
    else {
      rc = EOF;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fgetc
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fgetc(FILE *stream);
 */
static int default_c99_handler_fgetc(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = fgetc(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fgets
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	char *fgets(char *s, int size, FILE *stream);
 */
static int default_c99_handler_fgets(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *s;
    FILE *stream;
    int size;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    s = GET_LS_PTR(arg0->slot[0]);
    size = arg1->slot[0];
    stream = get_FILE(arg2->slot[0]);
    rc = (fgets(s, size, stream) == s) ? arg0->slot[0] : 0;
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fileno
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE library operation, implementing:
 *
 *      int fileno(FILE *stream);
 */
static int default_c99_handler_fileno(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = fileno(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fputc
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fputc(int c, FILE *stream);
 */
static int default_c99_handler_fputc(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *stream;
    int c;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    c = arg0->slot[0];
    stream = get_FILE(arg1->slot[0]);
    rc = fputc(c, stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fputs
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fputs(const char *s, FILE *stream);
 */
static int default_c99_handler_fputs(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *s;
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    s = GET_LS_PTR(arg0->slot[0]);
    stream = get_FILE(arg1->slot[0]);
    rc = fputs(s, stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_getc
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int getc(FILE *stream);
 */
static int default_c99_handler_getc(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = getc(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_getchar
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int getchar(void);
 */
static int default_c99_handler_getchar(char *ls, unsigned long opdata)
{
    DECL_0_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(SPE_STDIN);
    rc = getc(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_gets
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	char *gets(char *s);
 *
 * Implementation note: gets() is a classic security/integrity
 * hole, since its impossible to tell how many characters will
 * be input.
 */
static int default_c99_handler_gets(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    char *s, *r;
    int rc;
    int size;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(SPE_STDIN);
    s = GET_LS_PTR(arg0->slot[0]);
    size = LS_SIZE - arg0->slot[0];
    r = fgets(s, size, stream);
    rc = (r == s) ? arg0->slot[0] : 0;
    if (r == s) { /* remove trailing linefeed character. */
      char *p = s + strlen(s);
      if (p > s && p[-1] == '\n') p[-1] = '\0';
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_putc
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int putc(int c, FILE *stream);
 */
static int default_c99_handler_putc(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *f;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    f = get_FILE(arg1->slot[0]);
    rc = putc(arg0->slot[0], f);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_putchar
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int putchar(int c);
 */
static int default_c99_handler_putchar(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(SPE_STDOUT);
    rc = putc(arg0->slot[0], stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_puts
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	 int puts(const char *s);
 */
static int default_c99_handler_puts(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *s;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    s = GET_LS_PTR(arg0->slot[0]);
    rc = puts(s);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_ungetc
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 * 	int ungetc(int c, FILE *stream);
 */
static int default_c99_handler_ungetc(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *stream;
    int rc, c;

    DEBUG_PRINTF("%s\n", __func__);
    c = arg0->slot[0];
    stream = get_FILE(arg1->slot[0]);
    rc = ungetc(c, stream);
    PUT_LS_RC(rc, 0, 0, 0);
    return 0;
}

/**
 * default_c99_handler_fread
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
 */
static int default_c99_handler_fread(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    void *ptr;
    size_t size, nmemb;
    FILE *stream;
    int rc = 0;

    DEBUG_PRINTF("%s\n", __func__);
    ptr = GET_LS_PTR(arg0->slot[0]);
    size = arg1->slot[0];
    nmemb = arg2->slot[0];
    stream = get_FILE(arg3->slot[0]);
    rc = fread(ptr, size, nmemb, stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fwrite
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
 */
static int default_c99_handler_fwrite(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    void *ptr;
    size_t size, nmemb;
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    ptr = GET_LS_PTR(arg0->slot[0]);
    size = arg1->slot[0];
    nmemb = arg2->slot[0];
    stream = get_FILE(arg3->slot[0]);
    rc = fwrite(ptr, size, nmemb, stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fgetpos
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fgetpos(FILE *stream, fpos_t *pos);
 */
static int default_c99_handler_fgetpos(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *stream;
    fpos_t *pos;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    pos = (fpos_t *) GET_LS_PTR(arg1->slot[0]);
    rc = fgetpos(stream, pos);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fseek
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fseek(FILE *stream, long offset, int whence);
 */
static int default_c99_handler_fseek(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    FILE *stream;
    long offset;
    int whence;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    offset = (long) arg1->slot[0];
    whence = arg2->slot[0];
    rc = fseek(stream, offset, whence);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_fsetpos
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int fsetpos(FILE *stream, fpos_t *pos);
 */
static int default_c99_handler_fsetpos(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    FILE *stream;
    fpos_t *pos;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    pos = (fpos_t *) GET_LS_PTR(arg1->slot[0]);
    rc = fsetpos(stream, pos);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_ftell
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	long ftell(FILE *stream);
 */
static int default_c99_handler_ftell(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = ftell(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_rewind
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	void rewind(FILE *stream);
 */
static int default_c99_handler_rewind(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    FILE *stream;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rewind(stream);
    return 0;
}

/**
 * default_c99_handler_clearerr
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	void clearerr(FILE *stream);
 */
static int default_c99_handler_clearerr(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    FILE *stream;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    clearerr(stream);
    return 0;
}

/**
 * default_c99_handler_feof
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int feof(FILE *stream);
 */
static int default_c99_handler_feof(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = feof(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_ferror
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	void ferror(FILE *stream);
 */
static int default_c99_handler_ferror(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    FILE *stream;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    stream = get_FILE(arg0->slot[0]);
    rc = ferror(stream);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_c99_handler_perror
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data, plus the value of errno on the SPU
 * must be passed.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *  void perror(const char *s);
 */
static int default_c99_handler_perror(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    char *s;

    DEBUG_PRINTF("%s\n", __func__);
    s = GET_LS_PTR_NULL(arg0->slot[0]);
    errno = arg1->slot[0];
    /*
     * Older versions did not pass errno, so using older SPU newlib with
     * current libspe will likely give ouput like "Unknown error 262016".
     */
    perror(s);
    return 0;
}

/**
 * default_c99_handler_system
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * SPE C99 library operation, per: ISO/IEC C Standard 9899:1999,
 * implementing:
 *
 *	int system(const char *string);
 */
static int default_c99_handler_system(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *string;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    string = GET_LS_PTR(arg0->slot[0]);
    rc = system(string);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

static int (*default_c99_funcs[SPE_C99_NR_OPCODES]) (char *, unsigned long) = {
	[SPE_C99_UNUSED]	= NULL,
	[SPE_C99_CLEARERR]	= default_c99_handler_clearerr,
	[SPE_C99_FCLOSE]	= default_c99_handler_fclose,
	[SPE_C99_FEOF]		= default_c99_handler_feof,
	[SPE_C99_FERROR]	= default_c99_handler_ferror,
	[SPE_C99_FFLUSH]	= default_c99_handler_fflush,
	[SPE_C99_FGETC]		= default_c99_handler_fgetc,
	[SPE_C99_FGETPOS]	= default_c99_handler_fgetpos,
	[SPE_C99_FGETS]		= default_c99_handler_fgets,
	[SPE_C99_FILENO]	= default_c99_handler_fileno,
	[SPE_C99_FOPEN]		= default_c99_handler_fopen,
	[SPE_C99_FPUTC]		= default_c99_handler_fputc,
	[SPE_C99_FPUTS]		= default_c99_handler_fputs,
	[SPE_C99_FREAD]		= default_c99_handler_fread,
	[SPE_C99_FREOPEN]	= default_c99_handler_freopen,
	[SPE_C99_FSEEK]		= default_c99_handler_fseek,
	[SPE_C99_FSETPOS]	= default_c99_handler_fsetpos,
	[SPE_C99_FTELL]		= default_c99_handler_ftell,
	[SPE_C99_FWRITE]	= default_c99_handler_fwrite,
	[SPE_C99_GETC]		= default_c99_handler_getc,
	[SPE_C99_GETCHAR]	= default_c99_handler_getchar,
	[SPE_C99_GETS]		= default_c99_handler_gets,
	[SPE_C99_PERROR]	= default_c99_handler_perror,
	[SPE_C99_PUTC]		= default_c99_handler_putc,
	[SPE_C99_PUTCHAR]	= default_c99_handler_putchar,
	[SPE_C99_PUTS]		= default_c99_handler_puts,
	[SPE_C99_REMOVE]	= default_c99_handler_remove,
	[SPE_C99_RENAME]	= default_c99_handler_rename,
	[SPE_C99_REWIND]	= default_c99_handler_rewind,
	[SPE_C99_SETBUF]	= default_c99_handler_setbuf,
	[SPE_C99_SETVBUF]	= default_c99_handler_setvbuf,
	[SPE_C99_SYSTEM]	= default_c99_handler_system,
	[SPE_C99_TMPFILE]	= default_c99_handler_tmpfile,
	[SPE_C99_TMPNAM]	= default_c99_handler_tmpnam,
	[SPE_C99_UNGETC]	= default_c99_handler_ungetc,
	[SPE_C99_VFPRINTF]	= default_c99_handler_vfprintf,
	[SPE_C99_VFSCANF]	= default_c99_handler_vfscanf,
	[SPE_C99_VPRINTF]	= default_c99_handler_vprintf,
	[SPE_C99_VSCANF]	= default_c99_handler_vscanf,
	[SPE_C99_VSNPRINTF]	= default_c99_handler_vsnprintf,
	[SPE_C99_VSPRINTF]	= default_c99_handler_vsprintf,
	[SPE_C99_VSSCANF]	= default_c99_handler_vsscanf,
};

/**
 * default_c99_handler
 * @ls: base pointer to SPE local-store area.
 * @opdata: per C99 call opcode & data.
 *
 * Top-level dispatch for SPE C99 library operations.
 */
int _base_spe_default_c99_handler(unsigned long *base, unsigned long offset)
{
    int op, opdata; 

    if (!base) {
	DEBUG_PRINTF("%s: mmap LS required.\n", __func__);
	return 1;
    }
    offset = (offset & LS_ADDR_MASK) & ~0x1;
    opdata = *((int *)((char *) base + offset));
    op = SPE_C99_OP(opdata);
    if ((op <= 0) || (op >= SPE_C99_NR_OPCODES)) {
        DEBUG_PRINTF("%s: Unhandled type %08x\n", __func__,
                     SPE_C99_OP(opdata));
        return 1;
    }

    default_c99_funcs[op] ((char *) base, opdata);

    return 0;
}
