/* default_posix1_handler.c - emulate SPE POSIX.1 library calls.
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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/timex.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <linux/limits.h>

#include "default_posix1_handler.h"
#include "handler_utils.h"

/* SPE POSIX.1 Handlers - Overview:
 * This file implements handlers for SPE POSIX.1 library calls such as 
 * open(2), using C library facilities available on PPE.  
 *
 * The main contribution of this layer is to provide the SPE programer
 * with a set of utilities that operate on a large effective address
 * space, rather than LS objects.  For instance this feature can be
 * used to to mmap(2) a data file into an address space and then 
 * multi-buffer data into LS in order to avoid staging via expensive
 * buffered-I/O calls (e.g. fread(3)).
 *
 * SPE-side:
 * The SPE-side of this interface is implemented with a set of small stub
 * routines, which may be packaged into an SPE POSIX.1-compatible library.
 * These stubs are responsible for pushing SPE register parameters onto the
 * stack, and then executing a stop-and-signal instruction with a reserved
 * signal code (currently 0x2101).  By convention, the next word following
 * the stop-and-signal instruction will contain the POSIX.1 op-code along
 * with a pointer to the beginning of the call stack frame.
 *
 * PPE-side:
 * The PPE application or library receives stop-and-signal notification
 * from the OS.  The default_posix1_handler() routine is then called in order
 * to parse the POSIX.1 op-code and branch to a specific POSIX.1 handler,
 * such as default_posix1_handler_open().
 *
 * The POSIX.1 handlers use direct-mapped access to LS in order to fetch
 * parameters from the stack frame, and to place return values (including
 * errno) into the expected locations.
 *
 * Errata:
 * This code is (hopefully) just temporary, until built-in OS support for 
 * SPE syscalls is implemented.
 *
 * Enable SPE_POSIX1_SHM_CALLS if the various shm_ operations are needed
 * by the SPE.  This requires applications link with -lrt, so it is
 * currently disabled by default, and will return ENOSYS back to the
 * SPE.
 */


#define SPE_POSIX1_OP_SHIFT  24
#define SPE_POSIX1_OP_MASK   0xff
#define SPE_POSIX1_DATA_MASK 0xffffff
#define SPE_POSIX1_OP(_v)   (((_v) >> SPE_POSIX1_OP_SHIFT) & SPE_POSIX1_OP_MASK)
#define SPE_POSIX1_DATA(_v) ((_v) & SPE_POSIX1_DATA_MASK)

enum {
	SPE_POSIX1_ADJTIMEX=0x1,
	SPE_POSIX1_CLOSE,
	SPE_POSIX1_CREAT,
	SPE_POSIX1_FSTAT,
	SPE_POSIX1_FTOK	,
	SPE_POSIX1_GETPAGESIZE,
	SPE_POSIX1_GETTIMEOFDAY,
	SPE_POSIX1_KILL,
	SPE_POSIX1_LSEEK,
	SPE_POSIX1_LSTAT,
	SPE_POSIX1_MMAP,
	SPE_POSIX1_MREMAP,
	SPE_POSIX1_MSYNC,
	SPE_POSIX1_MUNMAP,
	SPE_POSIX1_OPEN,
	SPE_POSIX1_READ,
	SPE_POSIX1_SHMAT,
	SPE_POSIX1_SHMCTL,
	SPE_POSIX1_SHMDT,
	SPE_POSIX1_SHMGET,
	SPE_POSIX1_SHM_OPEN,
	SPE_POSIX1_SHM_UNLINK,
	SPE_POSIX1_STAT,
	SPE_POSIX1_UNLINK,
	SPE_POSIX1_WAIT,
	SPE_POSIX1_WAITPID,
	SPE_POSIX1_WRITE,
	/* added to support fortran frontend */
	SPE_POSIX1_FTRUNCATE,
	SPE_POSIX1_ACCESS,
	SPE_POSIX1_DUP,
	SPE_POSIX1_TIME,
	SPE_POSIX1_LAST_OPCODE,
};
#define SPE_POSIX1_NR_OPCODES	\
	((SPE_POSIX1_LAST_OPCODE - SPE_POSIX1_ADJTIMEX) + 1)

#define CHECK_POSIX1_OPCODE(_op)                                          \
    if (SPE_POSIX1_OP(opdata) != SPE_POSIX1_ ## _op) {                    \
        DEBUG_PRINTF("OPCODE (0x%x) mismatch.\n", SPE_POSIX1_OP(opdata)); \
        return 1;                                                         \
    }

typedef union {
    unsigned long long all64;
    unsigned int by32[2];
} addr64;

/* SPE-compatable stat structure. */
struct spe_compat_stat {
    unsigned int dev;
    unsigned int ino;
    unsigned int mode;
    unsigned int nlink;
    unsigned int uid;
    unsigned int gid;
    unsigned int rdev;
    unsigned int size;
    unsigned int blksize;
    unsigned int blocks;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
};

/* SPE-compatable timeval structure. */
struct spe_compat_timeval {
    int tv_sec;   
    int tv_usec; 
};

/* SPE-compatable timezone structure. */
struct spe_compat_timezone {
    int  tz_minuteswest;
    int  tz_dsttime;   
};

/* SPE-compatable timex structure. */
struct spe_compat_timex {
        unsigned int modes;
        int offset;
        int freq; 
        int maxerror;
        int esterror;
        int status; 
        int constant;
        int precision;
        int tolerance;
        struct spe_compat_timeval time; 
        int tick;
        int ppsfreq;
        int jitter;
        int shift; 
        int stabil;
        int jitcnt;
        int calcnt;
        int errcnt;
        int stbcnt;
        int  :32; int  :32; int  :32; int  :32;
        int  :32; int  :32; int  :32; int  :32;
        int  :32; int  :32; int  :32; int  :32;
};


/**
 * default_posix1_handler_stat
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int stat(const char *file_name, struct stat *buf)
 */
int default_posix1_handler_stat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct stat buf;
    struct spe_compat_stat spe_buf;
    char *file_name;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(STAT);
    file_name = GET_LS_PTR(arg0->slot[0]);
    rc = stat(file_name, &buf);
    if (rc == 0) {
    	spe_buf.dev     = buf.st_dev;
	spe_buf.ino     = buf.st_ino;
	spe_buf.mode    = buf.st_mode;
	spe_buf.nlink   = buf.st_nlink;
	spe_buf.uid     = buf.st_uid;
	spe_buf.gid     = buf.st_gid;
	spe_buf.rdev    = buf.st_rdev;
	spe_buf.size    = buf.st_size;
	spe_buf.blksize = buf.st_blksize;
	spe_buf.blocks  = buf.st_blocks;
	spe_buf.atime   = buf.st_atime;
	spe_buf.mtime   = buf.st_mtime;
	spe_buf.ctime   = buf.st_ctime;
	memcpy(GET_LS_PTR(arg1->slot[0]), &spe_buf, sizeof(spe_buf));
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_fstat
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int fstat(int filedes, struct stat *buf)
 */
int default_posix1_handler_fstat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct stat buf;
    struct spe_compat_stat spe_buf;
    int filedes;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(FSTAT);
    filedes = arg0->slot[0];
    rc = fstat(filedes, &buf);
    if (rc == 0) {
        spe_buf.dev     = buf.st_dev;
        spe_buf.ino     = buf.st_ino;
        spe_buf.mode    = buf.st_mode;
        spe_buf.nlink   = buf.st_nlink;
        spe_buf.uid     = buf.st_uid;
        spe_buf.gid     = buf.st_gid;
        spe_buf.rdev    = buf.st_rdev;
        spe_buf.size    = buf.st_size;
        spe_buf.blksize = buf.st_blksize;
        spe_buf.blocks  = buf.st_blocks;
        spe_buf.atime   = buf.st_atime;
        spe_buf.mtime   = buf.st_mtime;
        spe_buf.ctime   = buf.st_ctime;
	memcpy(GET_LS_PTR(arg1->slot[0]), &spe_buf, sizeof(spe_buf));
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_lstat
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int lstat(const char *file_name, struct stat *buf)
 */
int default_posix1_handler_lstat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct stat buf;
    struct spe_compat_stat spe_buf;
    char *file_name;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(LSTAT);
    file_name = GET_LS_PTR(arg0->slot[0]); 
    rc = lstat(file_name, &buf);
    if (rc == 0) {
        spe_buf.dev     = buf.st_dev;
        spe_buf.ino     = buf.st_ino;
        spe_buf.mode    = buf.st_mode;
        spe_buf.nlink   = buf.st_nlink;
        spe_buf.uid     = buf.st_uid;
        spe_buf.gid     = buf.st_gid;
        spe_buf.rdev    = buf.st_rdev;
        spe_buf.size    = buf.st_size;
        spe_buf.blksize = buf.st_blksize;
        spe_buf.blocks  = buf.st_blocks;
        spe_buf.atime   = buf.st_atime;
        spe_buf.mtime   = buf.st_mtime;
        spe_buf.ctime   = buf.st_ctime;
	memcpy(GET_LS_PTR(arg1->slot[0]), &spe_buf, sizeof(spe_buf));
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_open
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int open(const char *pathname, int flags, mode_t mode);
 */
int default_posix1_handler_open(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *pathname;
    int flags;
    mode_t mode;
    int rc;
    
    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(OPEN);
    pathname = GET_LS_PTR(arg0->slot[0]);
    flags = (int) arg1->slot[0];
    mode = (mode_t) arg2->slot[0];
    rc = open(pathname, flags, mode);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_access
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int access(const char *pathname, int mode);
 */
int default_posix1_handler_access(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    int mode;
    int rc;
    
    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(ACCESS);
    pathname = GET_LS_PTR(arg0->slot[0]);
    mode = (int) arg1->slot[0];
    rc = access(pathname, mode);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}


/**
 * default_posix1_handler_creat
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int creat(const char *pathname, mode_t mode);
 */
int default_posix1_handler_creat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    mode_t mode;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(CREAT);
    pathname = GET_LS_PTR(arg0->slot[0]);
    mode = (mode_t) arg1->slot[0];
    rc = creat(pathname, mode);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_close
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int close(int fd);
 */
int default_posix1_handler_close(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int fd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(CLOSE);
    fd = arg0->slot[0];
    rc = close(fd);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_dup
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int dup(int oldfd);
 */
int default_posix1_handler_dup(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int oldfd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(DUP);
    oldfd = arg0->slot[0];
    rc = dup(oldfd);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_time
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	time_t time(time_t *t);
 */
int default_posix1_handler_time(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    time_t *t;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(TIME);
    t = GET_LS_PTR(arg0->slot[0]);
    rc = time(t);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}


/**
 * default_posix1_handler_read
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	ssize_t read(int fd, void *buf, size_t count);
 */
int default_posix1_handler_read(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    void *buf;
    size_t count;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(READ);
    fd = arg0->slot[0];
    buf = GET_LS_PTR(arg1->slot[0]);
    count = arg2->slot[0];
    rc = read(fd, buf, count);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_write
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	ssize_t write(int fd, const void *buf, size_t count);
 */
int default_posix1_handler_write(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    void *buf;
    size_t count;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(WRITE);
    fd = arg0->slot[0];
    buf = GET_LS_PTR(arg1->slot[0]);
    count = arg2->slot[0];
    rc = write(fd, buf, count);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_lseek
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	off_t lseek(int fildes, off_t offset, int whence);
 */
int default_posix1_handler_lseek(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fildes;
    off_t offset;
    int whence;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(LSEEK);
    fildes = arg0->slot[0];
    offset = (off_t) arg1->slot[0];
    whence = arg2->slot[0];
    rc = lseek(fildes, offset, whence);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_ftruncate
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int ftruncate(int fd, off_t length);
 * 
 */
int default_posix1_handler_ftruncate(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    int fd;
    off_t length;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(FTRUNCATE);
    fd = arg0->slot[0];
    length = (off_t) arg1->slot[0];
    rc = ftruncate(fd, length);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_unlink
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int unlink(const char *pathname);
 */
int default_posix1_handler_unlink(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *pathname;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(UNLINK);
    pathname = GET_LS_PTR(arg0->slot[0]);
    rc = unlink(pathname);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_mmap
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	void  *mmap(void *start, size_t length, int prot , int flags, 
 *		    int fd, off_t offset);
 *
 * Both 'start' and 'addr' (returned) are treated as 64b EA pointers 
 * rather than LS offsets.  On powerpc32 ABI (which is ILP-32), these 
 * are handled as 32b EA pointers.
 */
int default_posix1_handler_mmap(char *ls, unsigned long opdata)
{
    DECL_6_ARGS();
    DECL_RET();
    addr64 start;
    size_t length;
    int prot;
    int flags;
    int fd;
    off_t offset;
    void *mmap_addr;
    addr64 addr;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(MMAP);
    start.by32[0] = arg0->slot[0];
    start.by32[1] = arg0->slot[1];
    length = (size_t) arg1->slot[0];
    prot = arg2->slot[0];
    flags = arg3->slot[0];
    fd = arg4->slot[0];
    offset = (off_t) arg5->slot[0];
    mmap_addr = mmap((void *) ((unsigned long) start.all64), 
			       length, prot, flags, fd, offset);
    if (mmap_addr == MAP_FAILED) {
	addr.all64 = -1LL;
    } else {
	addr.all64 = (unsigned long long) (unsigned long) mmap_addr;
    }
    PUT_LS_RC(addr.by32[0], addr.by32[1], 0, errno);
    return 0;
}

/**
 * default_posix1_handler_munmap
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int munmap(void *start, size_t length);
 *
 * For this interface, 'start' is taken to be an EA pointer
 * rather than an LS offset.
 */
int default_posix1_handler_munmap(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    addr64 start;
    size_t length;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(MUNMAP);
    start.by32[0] = arg0->slot[0];
    start.by32[1] = arg0->slot[1];
    length = (size_t) arg1->slot[0];
    rc = munmap((void *) ((unsigned long) start.all64), length);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_mremap
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	void *mremap(void *old_address, size_t old_size, 
 *		     size_t new_size, unsigned long flags);
 *
 * For this interface, both 'old_address' and the returned value
 * are taken to be EA pointers, rather than LS offsets.
 */
int default_posix1_handler_mremap(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    addr64 old_address;
    size_t old_size;
    size_t new_size;
    unsigned long flags;
    addr64 addr;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(MREMAP);
    old_address.by32[0] = arg0->slot[0];
    old_address.by32[1] = arg0->slot[1];
    old_size = (size_t) arg1->slot[0];
    new_size = (size_t) arg2->slot[0];
    flags = (unsigned long) arg3->slot[0];
    addr.all64 = (unsigned long) 
                           mremap((void *) ((unsigned long) old_address.all64),
                                  old_size, new_size, flags);
    PUT_LS_RC(addr.by32[0], addr.by32[1], 0, errno);
    return 0;
}

/**
 * default_posix1_handler_msync
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int msync(const void *start, size_t length, int flags);
 *
 * For this interface, 'start' is taken to be an EA pointer
 * rather than an LS offset.
 */
int default_posix1_handler_msync(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    addr64 start;
    size_t length;
    int flags;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(MSYNC);
    start.by32[0] = arg0->slot[0];
    start.by32[1] = arg0->slot[1];
    length = (size_t) arg1->slot[0];
    flags = arg2->slot[0];
    rc = msync((void *) ((unsigned long) start.all64), length, flags);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_shm_open
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	void *shm_open(const char *name, int oflag, mode_t mode);
 *
 * For this interface, the returned value is taken to be an
 * EA pointer rather than an LS offset.
 */
int default_posix1_handler_shm_open(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    unsigned char *name;
    int oflag;
    mode_t mode;
    unsigned long addr;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(SHM_OPEN);
    name = GET_LS_PTR(arg0->slot[0]);
    oflag = (int) arg1->slot[0];
    mode = (mode_t) arg2->slot[0];
    addr = (unsigned long) shm_open(name, oflag, mode);
    PUT_LS_RC(addr, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
    CHECK_POSIX1_OPCODE(SHM_OPEN);
    PUT_LS_RC(-1, 0, 0, ENOSYS);
#endif
    return 0;
}

/**
 * default_posix1_handler_shm_unlink
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int shm_unlink(const char *name);
 */
int default_posix1_handler_shm_unlink(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_1_ARGS();
    DECL_RET();
    unsigned char *name;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(SHM_UNLINK);
    name = GET_LS_PTR(arg0->slot[0]);
    rc = shm_unlink(name);
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
    CHECK_POSIX1_OPCODE(SHM_UNLINK);
    PUT_LS_RC(-1, 0, 0, ENOSYS);
#endif
    return 0;
}

/**
 * default_posix1_handler_shmat
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	void *shmat(int shmid, const void *shmaddr, int shmflg);
 *
 * For this interface, both 'shmaddr' and the returned value are
 * taken to be EA pointers rather than LS offsets.
 */
int default_posix1_handler_shmat(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    int shmid;
    addr64 shmaddr;
    int shmflg;
    addr64 addr;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(SHMAT);
    shmid = arg0->slot[0];
    shmaddr.by32[0] = arg1->slot[0];
    shmaddr.by32[1] = arg1->slot[1];
    shmflg = arg2->slot[0];
    addr.all64 = (unsigned long) 
		   shmat(shmid, (void *)((unsigned long)shmaddr.all64), shmflg);
    PUT_LS_RC(addr.by32[0], addr.by32[1], 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
    CHECK_POSIX1_OPCODE(SHMAT);
    PUT_LS_RC(-1, 0, 0, ENOSYS);
#endif
    return 0;
}

/**
 * default_posix1_handler_shmdt
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int shmdt(const void *shmaddr);
 *
 * For this interface 'shmaddr' is taken to be an EA pointer
 * rather than an LS offset.
 */
int default_posix1_handler_shmdt(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_1_ARGS();
    DECL_RET();
    addr64 shmaddr;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(SHMDT);
    shmaddr.by32[0] = arg0->slot[0];
    shmaddr.by32[1] = arg0->slot[1];
    rc = shmdt((void *) ((unsigned long) shmaddr.all64));
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
    CHECK_POSIX1_OPCODE(SHMDT);
    PUT_LS_RC(-1, 0, 0, ENOSYS);
#endif
    return 0;
}

/**
 * default_posix1_handler_shmctl
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int shmctl(int shmid, int cmd, struct shmid_ds *buf);
 *
 * For this interface 'buf' is taken to be an EA pointer rather
 * than an LS offset.
 */
int default_posix1_handler_shmctl(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    int shmid;
    int cmd;
    addr64 buf;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(SHMCTL);
    shmid = arg0->slot[0];
    cmd = arg1->slot[0];
    buf.by32[0] = arg2->slot[0];
    buf.by32[1] = arg2->slot[1];
    rc = shmctl(shmid, cmd, (struct shmid_ds *) ((unsigned long) buf.all64));
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
    CHECK_POSIX1_OPCODE(SHMCTL);
    PUT_LS_RC(-1, 0, 0, ENOSYS);
#endif
    return 0;
}

/**
 * default_posix1_handler_shmget
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	int shmget(key_t key, int size, int shmflg);
 */
int default_posix1_handler_shmget(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    key_t key;
    int size;
    int shmflg;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(SHMGET);
    key = (key_t) arg0->slot[0];
    size = arg1->slot[0];
    shmflg = arg2->slot[0];
    rc = shmget(key, size, shmflg);
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
    CHECK_POSIX1_OPCODE(SHMGET);
    PUT_LS_RC(-1, 0, 0, ENOSYS);
#endif
    return 0;
}

/**
 * default_posix1_handler_ftok
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *	key_t ftok(const char *pathname, int proj_id);
 *
 * For convenience, this function takes 'pathname' to be the LS 
 * offset of a NULL-terminated character string.
 */
int default_posix1_handler_ftok(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    int proj_id;
    key_t rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(FTOK);
    pathname = GET_LS_PTR(arg0->slot[0]);
    proj_id = arg1->slot[0];
    rc = ftok(pathname, proj_id);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_getpagesize
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int getpagesize(void);
 */
int default_posix1_handler_getpagesize(char *ls, unsigned long opdata)
{
    DECL_0_ARGS();
    DECL_RET();
    int sz;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(GETPAGESIZE);
    sz = getpagesize();
    PUT_LS_RC(sz, 0, 0, 0);
    return 0;
}

/**
 * default_posix1_handler_wait
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      pid_t wait(int *status)
 */
int default_posix1_handler_wait(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int *status;
    pid_t pid;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(WAIT);
    status = (int *) GET_LS_PTR(arg0->slot[0]);
    pid = wait(status);
    PUT_LS_RC(pid, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_waitpid
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      pid_t waitpid(pid_t pid, int *status, int options)
 */
int default_posix1_handler_waitpid(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    pid_t pid;
    int *status;
    int options;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(WAIT);
    pid = arg0->slot[0];
    status = (int *) GET_LS_PTR(arg1->slot[0]);
    options = arg2->slot[0];
    pid = waitpid(pid, status, options);
    PUT_LS_RC(pid, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_kill
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int kill(pid_t pid, int sig)
 */
int default_posix1_handler_kill(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    pid_t pid;
    int sig;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(KILL);
    pid = arg0->slot[0];
    sig = arg1->slot[0];
    rc = kill(pid, sig);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

#define _COPY_TIMEX_FIELD(_dst, _src, _field)		\
	(_dst)->_field = (_src)->_field

#define _COPY_TIMEX_STRUCT(_dst, _src)			\
	_COPY_TIMEX_FIELD(_dst, _src, modes);		\
	_COPY_TIMEX_FIELD(_dst, _src, offset);		\
	_COPY_TIMEX_FIELD(_dst, _src, freq);		\
	_COPY_TIMEX_FIELD(_dst, _src, maxerror);	\
	_COPY_TIMEX_FIELD(_dst, _src, esterror);	\
	_COPY_TIMEX_FIELD(_dst, _src, status);		\
	_COPY_TIMEX_FIELD(_dst, _src, constant);	\
	_COPY_TIMEX_FIELD(_dst, _src, precision);	\
	_COPY_TIMEX_FIELD(_dst, _src, tolerance);	\
	_COPY_TIMEX_FIELD(_dst, _src, time.tv_sec);	\
	_COPY_TIMEX_FIELD(_dst, _src, time.tv_usec);	\
	_COPY_TIMEX_FIELD(_dst, _src, tick);		\
	_COPY_TIMEX_FIELD(_dst, _src, ppsfreq);		\
	_COPY_TIMEX_FIELD(_dst, _src, jitter);		\
	_COPY_TIMEX_FIELD(_dst, _src, shift);		\
	_COPY_TIMEX_FIELD(_dst, _src, stabil);		\
	_COPY_TIMEX_FIELD(_dst, _src, jitcnt);		\
	_COPY_TIMEX_FIELD(_dst, _src, calcnt);		\
	_COPY_TIMEX_FIELD(_dst, _src, errcnt);		\
	_COPY_TIMEX_FIELD(_dst, _src, stbcnt)

/**
 * default_posix1_handler_adjtimex
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE adjtimex library operation:
 *
 *      int adjtimex(struct timex *buf)
 */
int default_posix1_handler_adjtimex(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    struct spe_compat_timex *buf;
    struct timex local;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(ADJTIMEX);
    buf = (struct spe_compat_timex *) GET_LS_PTR(arg0->slot[0]);
    _COPY_TIMEX_STRUCT(&local, buf);
    rc = adjtimex(&local);
    if (rc==0) {
    	_COPY_TIMEX_STRUCT(buf, &local);
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_gettimeofday
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * SPE POSIX.1 library operation, per: POSIX.1 (IEEE Std 1003.1),
 * implementing:
 *
 *      int gettimeofday(struct timeval *tv, struct timezone *tz)
 */
int default_posix1_handler_gettimeofday(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct spe_compat_timeval *tv;
    struct spe_compat_timezone *tz;
    struct timeval t;
    struct timezone zone;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    CHECK_POSIX1_OPCODE(GETTIMEOFDAY);
    tv = (struct spe_compat_timeval *) GET_LS_PTR(arg0->slot[0]);
    tz = (struct spe_compat_timezone *) GET_LS_PTR(arg1->slot[0]);
    rc = gettimeofday(&t, &zone);
    if (rc==0) {
	if (arg0->slot[0] != 0) {
	    tv->tv_sec = t.tv_sec;
	    tv->tv_usec = t.tv_usec;
	}
	if (arg1->slot[0] != 0) {
	    tz->tz_minuteswest = zone.tz_minuteswest;
	}
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

int (*default_posix1_funcs[SPE_POSIX1_NR_OPCODES]) (char *, unsigned long) = {
	default_posix1_handler_adjtimex,
	default_posix1_handler_close,
	default_posix1_handler_creat,
	default_posix1_handler_fstat,
	default_posix1_handler_ftok,
	default_posix1_handler_getpagesize,
	default_posix1_handler_gettimeofday,
	default_posix1_handler_kill,
	default_posix1_handler_lseek,
	default_posix1_handler_lstat,
	default_posix1_handler_mmap,
	default_posix1_handler_mremap,
	default_posix1_handler_msync,
	default_posix1_handler_munmap,
	default_posix1_handler_open,
	default_posix1_handler_read,
	default_posix1_handler_shmat,
	default_posix1_handler_shmctl,
	default_posix1_handler_shmdt,
	default_posix1_handler_shmget,
	default_posix1_handler_shm_open,
	default_posix1_handler_shm_unlink,
	default_posix1_handler_stat,
	default_posix1_handler_unlink,
	default_posix1_handler_wait,
	default_posix1_handler_waitpid,
	default_posix1_handler_write,
	default_posix1_handler_ftruncate,
	default_posix1_handler_access,
	default_posix1_handler_dup,
	default_posix1_handler_time,
};

/**
 * default_posix1_handler
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Default POSIX.1 call dispatch function.
 */
int default_posix1_handler(char *base, unsigned long offset)
{
    int op, opdata;

    if (!base) {
        DEBUG_PRINTF("%s: mmap LS required.\n", __func__);
        return 1;
    }
    offset = (offset & LS_ADDR_MASK) & ~0x1;
    opdata = *((int *)((char *) base + offset));
    op = SPE_POSIX1_OP(opdata);
    if ((op <= 0) || (op >= SPE_POSIX1_NR_OPCODES)) {
        DEBUG_PRINTF("%s: Unhandled type opdata=%08x op=%08x\n", __func__,
                     opdata, SPE_POSIX1_OP(opdata));
        return 1;
    }

    default_posix1_funcs[op-1] (base, opdata);

    return 0;
}

