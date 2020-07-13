#define _GNU_SOURCE

#include "default_libea_handler.h"
#include "handler_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>

typedef union {
    unsigned long long all64;
    unsigned int by32[2];
} addr64;

static inline size_t arg64_to_size_t(struct spe_reg128* arg0){
  addr64 argument0;
  argument0.by32[0] = arg0->slot[0];
  argument0.by32[1] = arg0->slot[1];
  return (size_t)argument0.all64;
}
/**
 * default_libea_handler_calloc
 * @ls: base pointer to local store area.
 * @opdata: LIBEA call opcode & data.
 *
 * LIBEA library operation, per: upcoming RFC based on POSIX,C99
 * implementing:
 *
 *	void  *calloc(size_t mnemb, size_t size);
 *
 * 'addr' (returned) is treated as a 64b EA pointer
 * rather than LS offset.  On powerpc32 ABI (which is ILP-32), this
 * is handled as a 32b EA pointer.
 */
static int default_libea_handler_calloc(char *ls, unsigned long opdata)
{
  DECL_2_ARGS();
  DECL_RET();
  size_t nmemb;
  size_t size;
  void* calloc_addr = NULL;
  addr64 ret2;

  nmemb = arg64_to_size_t(arg0);
  size = arg64_to_size_t(arg1);


  /* OK, now if we are 32 bit and we were passed 64 bit that really was
   * bigger than 32 bit we need to bail.
   */
  if((arg0->slot[0]== 0 && arg1->slot[0] == 0) || sizeof(nmemb) == 8)
    calloc_addr = calloc(nmemb, size);
  else
    errno = ENOMEM;

  ret2.all64 = (unsigned long long) (unsigned long) calloc_addr;

  PUT_LS_RC(ret2.by32[0], ret2.by32[1], 0, errno);
  return 0;

}

/**
 * default_libea_handler_free
 * @ls: base pointer to local store area.
 * @opdata: LIBEA call opcode & data.
 *
 * LIBEA library operation, per: upcoming RFC based on POSIX,C99
 * implementing:
 *
 *	void free(void* ptr);
 *
 * 'ptr' is treated as a 64b EA pointer
 * rather than LS offset.  On powerpc32 ABI (which is ILP-32), this
 * is handled as a 32b EA pointer.
 */
static int default_libea_handler_free(char *ls, unsigned long opdata)
{
  DECL_1_ARGS();
  addr64 ptr;

  ptr.by32[0] = arg0->slot[0];
  ptr.by32[1] = arg0->slot[1];

  free((void *) ((unsigned long) ptr.all64));

  return 0;

}

/**
 * default_libea_handler_malloc
 * @ls: base pointer to local store area.
 * @opdata: LIBEA call opcode & data.
 *
 * LIBEA library operation, per: upcoming RFC based on POSIX,C99
 * implementing:
 *
 *	void  *malloc(size_t size);
 *
 * 'ret' (returned) is treated as a 64b EA pointer
 * rather than LS offset.  On powerpc32 ABI (which is ILP-32), this
 * is handled as a 32b EA pointer.
 */
static int default_libea_handler_malloc(char *ls, unsigned long opdata)
{
  DECL_1_ARGS();
  DECL_RET();
  size_t size;
  void* malloc_addr = NULL;
  addr64 ret2;

  size = arg64_to_size_t(arg0);

  if(arg0->slot[0] == 0 || sizeof(size) == 8)
    malloc_addr = malloc(size);
  else
    errno = ENOMEM;

  ret2.all64 = (unsigned long long) (unsigned long) malloc_addr;

  PUT_LS_RC(ret2.by32[0], ret2.by32[1], 0, errno);
  return 0;

}

/**
 * default_libea_handler_realloc
 * @ls: base pointer to local store area.
 * @opdata: LIBEA call opcode & data.
 *
 * LIBEA library operation, per: upcoming RFC based on POSIX,C99
 * implementing:
 *
 *	void  *realloc(void* ptr, size_t size);
 *
 * 'ptr' and 'ret' (returned) are treated as 64b EA pointers
 * rather than LS offsets.  On powerpc32 ABI (which is ILP-32), this
 * is handled as a 32b EA pointer.
 */
static int default_libea_handler_realloc(char *ls, unsigned long opdata)
{
  DECL_2_ARGS();
  DECL_RET();
  addr64 ptr;
  size_t size;
  void* realloc_addr;
  addr64 ret2;

  ptr.by32[0] = arg0->slot[0];
  ptr.by32[0] = arg0->slot[1];

  size = arg64_to_size_t(arg1);

  if(arg1->slot[0] == 0 || sizeof(size) == 8)
    realloc_addr = realloc((void *) ((unsigned long)ptr.all64), size);
  else
    errno = ENOMEM;

  ret2.all64 = (unsigned long long) (unsigned long) realloc_addr;

  PUT_LS_RC(ret2.by32[0], ret2.by32[1], 0, errno);
  return 0;

}

/**
 * default_libea_handler_posix_memalign
 * @ls: base pointer to local store area.
 * @opdata: LIBEA call opcode & data.
 *
 * LIBEA library operation, implement:
 *
 *          int posix_memalign(void **memptr, size_t alignment, size_t size);
 *
 * memptr is an LS pointer that points to an EA pointer, so *memptr is 32
 * or 64 bits long.
 */
static int default_libea_handler_posix_memalign(char *ls, unsigned long opdata)
{
  DECL_3_ARGS();
  DECL_RET();
  size_t size, alignment;
  void **memptr;
  int rc;

  memptr = GET_LS_PTR(arg0->slot[0]);
  alignment = arg64_to_size_t(arg1);
  size = arg64_to_size_t(arg2);

  if(arg2->slot[0] == 0 || sizeof(size) == 8)
    rc = posix_memalign(memptr, alignment, size);
  else
    /*
     * Yes, rc and not errno since posix_memalign never sets errno.
     */
    rc = ENOMEM;

  PUT_LS_RC(rc, 0, 0, 0);
  return 0;
}

static int (*default_libea_funcs[SPE_LIBEA_NR_OPCODES]) (char *, unsigned long) = {
	[SPE_LIBEA_UNUSED]		= NULL,
	[SPE_LIBEA_CALLOC]		= default_libea_handler_calloc,
	[SPE_LIBEA_FREE]		= default_libea_handler_free,
	[SPE_LIBEA_MALLOC]		= default_libea_handler_malloc,
	[SPE_LIBEA_REALLOC]		= default_libea_handler_realloc,
            [SPE_LIBEA_POSIX_MEMALIGN]          = default_libea_handler_posix_memalign,
};

/**
 * default_libea_handler
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Default POSIX.1 call dispatch function.
 */
int _base_spe_default_libea_handler(char *base, unsigned long offset)
{
    int op, opdata;

    if (!base) {
        DEBUG_PRINTF("%s: mmap LS required.\n", __func__);
        return 1;
    }
    offset = (offset & LS_ADDR_MASK) & ~0x1;
    opdata = *((int *)((char *) base + offset));
    op = SPE_LIBEA_OP(opdata);
    if ((op <= 0) || (op >= SPE_LIBEA_NR_OPCODES)) {
        DEBUG_PRINTF("%s: Unhandled type opdata=%08x op=%08x\n", __func__,
                     opdata, SPE_LIBEA_OP(opdata));
        return 1;
    }

    default_libea_funcs[op] (base, opdata);

    return 0;
}
