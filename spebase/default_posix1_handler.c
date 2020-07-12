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
#include <sys/uio.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdint.h>
#include <pthread.h>
#include <dirent.h>
#include <utime.h>

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
	SPE_POSIX1_UNUSED,
	SPE_POSIX1_ADJTIMEX,
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
	SPE_POSIX1_NANOSLEEP,
	SPE_POSIX1_CHDIR,
	SPE_POSIX1_FCHDIR,
	SPE_POSIX1_MKDIR,
	SPE_POSIX1_MKNOD,
	SPE_POSIX1_RMDIR,
	SPE_POSIX1_CHMOD,
	SPE_POSIX1_FCHMOD,
	SPE_POSIX1_CHOWN,
	SPE_POSIX1_FCHOWN,
	SPE_POSIX1_LCHOWN,
	SPE_POSIX1_GETCWD,
	SPE_POSIX1_LINK,
	SPE_POSIX1_SYMLINK,
	SPE_POSIX1_READLINK,
	SPE_POSIX1_SYNC,
	SPE_POSIX1_FSYNC,
	SPE_POSIX1_FDATASYNC,
	SPE_POSIX1_DUP2,
	SPE_POSIX1_LOCKF,
	SPE_POSIX1_TRUNCATE,
	SPE_POSIX1_MKSTEMP,
	SPE_POSIX1_MKTEMP,
	SPE_POSIX1_OPENDIR,
	SPE_POSIX1_CLOSEDIR,
	SPE_POSIX1_READDIR,
	SPE_POSIX1_REWINDDIR,
	SPE_POSIX1_SEEKDIR,
	SPE_POSIX1_TELLDIR,
	SPE_POSIX1_SCHED_YIELD,
	SPE_POSIX1_UMASK,
	SPE_POSIX1_UTIME,
	SPE_POSIX1_UTIMES,
	SPE_POSIX1_PREAD,
	SPE_POSIX1_PWRITE,
	SPE_POSIX1_READV,
	SPE_POSIX1_WRITEV,
	SPE_POSIX1_LAST_OPCODE,
};
#define SPE_POSIX1_NR_OPCODES	\
	((SPE_POSIX1_LAST_OPCODE - SPE_POSIX1_ADJTIMEX) + 1)

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

struct spe_compat_timespec {
    int tv_sec;
    int tv_nsec;
};

struct spe_compat_utimbuf {
    int actime;
    int modtime;
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

struct spe_compat_dirent {
        uint32_t d_ino;
        char d_name[256];
};

static inline void *spe_reg128toptr(struct spe_reg128 *reg)
{
    addr64 a64;

    a64.by32[0] = reg->slot[0];
    a64.by32[1] = reg->slot[1];
    return (void *) (unsigned long) a64.all64;
}

static inline uint64_t spe_ptrto64 (void *ptr)
{
    return (uint64_t) (unsigned long) ptr;
}

static void spe_copy_dirent(struct spe_compat_dirent *spe_ent,
                            struct dirent *ent)
{
    spe_ent->d_ino = ent->d_ino;
    memcpy(spe_ent->d_name, ent->d_name, sizeof(spe_ent->d_name));
}

/*
 * Needs to be compiled with -D_XOPEN_SOURCE to get IOV_MAX, but that
 * sounds bad: IOV_MAX is the maximum number of vectors for readv and
 * writev calls.
 */
#ifndef IOV_MAX
#define IOV_MAX	1024
#endif

#define SPU_SSIZE_MAX	0x7fffffffL

struct ls_iovec {
    uint32_t iov_base;
    uint32_t iov_len;
};

static int
check_conv_spuvec(char *ls, struct iovec *vec, struct ls_iovec *lsvec, int
          count)
{
    int i;
    size_t sum;

    if (count > IOV_MAX) {
        errno = EINVAL;
        return -1;
    }

    sum = 0;
    for (i = 0; i < count; i++) {
        vec[i].iov_base = GET_LS_PTR(lsvec[i].iov_base);
        vec[i].iov_len = lsvec[i].iov_len;
        if (sum + vec[i].iov_len > SPU_SSIZE_MAX) {
            errno = EINVAL;
            return -1;
        }
        sum += vec[i].iov_len;
    }
    return 0;
}

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
static int default_posix1_handler_stat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct stat buf;
    struct spe_compat_stat spe_buf;
    char *file_name;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_fstat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct stat buf;
    struct spe_compat_stat spe_buf;
    int filedes;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_lstat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct stat buf;
    struct spe_compat_stat spe_buf;
    char *file_name;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_open(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *pathname;
    int flags;
    mode_t mode;
    int rc;
    
    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_access(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    int mode;
    int rc;
    
    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_creat(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    mode_t mode;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_close(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int fd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_dup(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int oldfd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_time(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    time_t *t;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_read(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    void *buf;
    size_t count;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_write(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    void *buf;
    size_t count;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_lseek(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fildes;
    off_t offset;
    int whence;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fildes = arg0->slot[0];
    offset = (int) arg1->slot[0];
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
static int default_posix1_handler_ftruncate(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    int fd;
    off_t length;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    length = (int) arg1->slot[0];
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
static int default_posix1_handler_unlink(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *pathname;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_mmap(char *ls, unsigned long opdata)
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
    start.by32[0] = arg0->slot[0];
    start.by32[1] = arg0->slot[1];
    length = (size_t) arg1->slot[0];
    prot = arg2->slot[0];
    flags = arg3->slot[0];
    fd = arg4->slot[0];
    offset = (int) arg5->slot[0];
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
static int default_posix1_handler_munmap(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    addr64 start;
    size_t length;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_mremap(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    addr64 old_address;
    size_t old_size;
    size_t new_size;
    unsigned long flags;
    addr64 addr;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_msync(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    addr64 start;
    size_t length;
    int flags;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_shm_open(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    unsigned char *name;
    int oflag;
    mode_t mode;
    unsigned long addr;

    DEBUG_PRINTF("%s\n", __func__);
    name = GET_LS_PTR(arg0->slot[0]);
    oflag = (int) arg1->slot[0];
    mode = (mode_t) arg2->slot[0];
    addr = (unsigned long) shm_open(name, oflag, mode);
    PUT_LS_RC(addr, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
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
static int default_posix1_handler_shm_unlink(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_1_ARGS();
    DECL_RET();
    unsigned char *name;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    name = GET_LS_PTR(arg0->slot[0]);
    rc = shm_unlink(name);
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
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
static int default_posix1_handler_shmat(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    int shmid;
    addr64 shmaddr;
    int shmflg;
    addr64 addr;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_shmdt(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_1_ARGS();
    DECL_RET();
    addr64 shmaddr;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    shmaddr.by32[0] = arg0->slot[0];
    shmaddr.by32[1] = arg0->slot[1];
    rc = shmdt((void *) ((unsigned long) shmaddr.all64));
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
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
static int default_posix1_handler_shmctl(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    int shmid;
    int cmd;
    addr64 buf;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    shmid = arg0->slot[0];
    cmd = arg1->slot[0];
    buf.by32[0] = arg2->slot[0];
    buf.by32[1] = arg2->slot[1];
    rc = shmctl(shmid, cmd, (struct shmid_ds *) ((unsigned long) buf.all64));
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
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
static int default_posix1_handler_shmget(char *ls, unsigned long opdata)
{
#ifdef SPE_POSIX1_SHM_CALLS
    DECL_3_ARGS();
    DECL_RET();
    key_t key;
    int size;
    int shmflg;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    key = (key_t) arg0->slot[0];
    size = arg1->slot[0];
    shmflg = arg2->slot[0];
    rc = shmget(key, size, shmflg);
    PUT_LS_RC(rc, 0, 0, errno);
#else
    DECL_0_ARGS();
    DECL_RET();
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
static int default_posix1_handler_ftok(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    int proj_id;
    key_t rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_getpagesize(char *ls, unsigned long opdata)
{
    DECL_0_ARGS();
    DECL_RET();
    int sz;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_wait(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int *status;
    pid_t pid;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_waitpid(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    pid_t pid;
    int *status;
    int options;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_kill(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    pid_t pid;
    int sig;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_adjtimex(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    struct spe_compat_timex *buf;
    struct timex local;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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
static int default_posix1_handler_gettimeofday(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct spe_compat_timeval *tv;
    struct spe_compat_timezone *tz;
    struct timeval t;
    struct timezone zone;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
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

/**
 * default_posix1_handler_nanosleep
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int nanosleep(struct timespec *req, struct timespec *rem);
 */
static int default_posix1_handler_nanosleep(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    struct spe_compat_timespec *spereq;
    struct spe_compat_timespec *sperem;
    struct timespec req;
    struct timespec rem, *remp;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    spereq = GET_LS_PTR(arg0->slot[0]);
    req.tv_sec = spereq->tv_sec;
    req.tv_nsec = spereq->tv_nsec;
    if (arg1->slot[0] == 0) {
        sperem = NULL;
        remp = NULL;
    } else {
        sperem = GET_LS_PTR(arg1->slot[0]);
        remp = &rem;
    }
    rc = nanosleep(&req, remp);
    if (remp) {
        sperem->tv_sec = remp->tv_sec;
        sperem->tv_nsec = remp->tv_nsec;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_chdir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int chdir(const char *path)
 */

static int default_posix1_handler_chdir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *path;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    rc = chdir(path);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_fchdir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int fchdir(int fd)
 */
static int default_posix1_handler_fchdir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int fd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    rc = fchdir(fd);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_mkdir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int mkdir(const char *pathname, mode_t mode)
 */
static int default_posix1_handler_mkdir(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *pathname;
    mode_t mode;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    pathname = GET_LS_PTR(arg0->slot[0]);
    mode = arg1->slot[0];
    rc = mkdir(pathname, mode);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_mknod
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int mknod(const char *pathname, mode_t mode, dev_t dev)
 */
static int default_posix1_handler_mknod(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *pathname;
    mode_t mode;
    dev_t dev;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    pathname = GET_LS_PTR(arg0->slot[0]);
    mode = arg1->slot[0];
    dev = arg2->slot[0];
    rc = mknod(pathname, mode, dev);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_rmdir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int rmdir(const char *pathname)
 */
static int default_posix1_handler_rmdir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *pathname;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    pathname = GET_LS_PTR(arg0->slot[0]);
    rc = rmdir(pathname);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_chmod
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int chmod(const char *path, mode_t mode)
 */
static int default_posix1_handler_chmod(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *path;
    mode_t mode;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    mode = arg1->slot[0];
    rc = chmod(path, mode);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_fchmod
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int fchmod(int fildes, mode_t mode)
 */
static int default_posix1_handler_fchmod(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    int filedes;
    mode_t mode;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    filedes = arg0->slot[0];
    mode = arg1->slot[0];
    rc = fchmod(filedes, mode);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_chown
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int chown(const char *path, uid_t owner, gid_t group)
 */
static int default_posix1_handler_chown(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *path;
    uid_t owner;
    gid_t group;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    owner = arg1->slot[0];
    group = arg2->slot[0];
    rc = chown(path, owner, group);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_fchown
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int fchown(int fd, uid_t owner, gid_t group)
 */
static int default_posix1_handler_fchown(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    uid_t owner;
    gid_t group;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    owner = arg1->slot[0];
    group = arg2->slot[0];
    rc = fchown(fd, owner, group);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_lchown
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int lchown(const char *path, uid_t owner, gid_t group)
 */
static int default_posix1_handler_lchown(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *path;
    uid_t owner;
    gid_t group;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    owner = arg1->slot[0];
    group = arg2->slot[0];
    rc = lchown(path, owner, group);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_getcwd
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     char *getcwd(char *buf, size_t size)
 */
static int default_posix1_handler_getcwd(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *buf;
    size_t size;
    char *res;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    if (arg1->slot[0] == 0) {
        /*
         * Do not support a NULL buf, since any allocations need to take
         * place on the SPU.
         */
        PUT_LS_RC(0, 0, 0, EINVAL);
        return 0;
    }
    buf = GET_LS_PTR(arg0->slot[0]);
    size = arg1->slot[0];
    res = getcwd(buf, size);
    if (res == buf) {
        rc = arg0->slot[0];
    } else {
        rc = 0;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_link
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int link(const char *oldpath, const char *newpath)
 */
static int default_posix1_handler_link(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *oldpath;
    char *newpath;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    oldpath = GET_LS_PTR(arg0->slot[0]);
    newpath = GET_LS_PTR(arg1->slot[0]);
    rc = link(oldpath, newpath);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_symlink
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int symlink(const char *oldpath, const char *newpath)
 */
static int default_posix1_handler_symlink(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *oldpath;
    char *newpath;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    oldpath = GET_LS_PTR(arg0->slot[0]);
    newpath = GET_LS_PTR(arg1->slot[0]);
    rc = symlink(oldpath, newpath);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_readlink
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     ssize_t readlink(const char *path, char *buf, size_t bufsiz)
 */
static int default_posix1_handler_readlink(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    char *path;
    char *buf;
    size_t bufsiz;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    buf = GET_LS_PTR(arg1->slot[0]);
    bufsiz = arg2->slot[0];
    rc = readlink(path, buf, bufsiz);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_sync
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      void sync(void)
 */
static int default_posix1_handler_sync(char *ls, unsigned long opdata)
{
    DEBUG_PRINTF("%s\n", __func__);
    sync();
    return 0;
}

/**
 * default_posix1_handler_fsync
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      int fsync(int fd)
 */
static int default_posix1_handler_fsync(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int fd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    rc = fsync(fd);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_fdatasync
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      int fdatasync(int fd)
 */
static int default_posix1_handler_fdatasync(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    int fd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    rc = fdatasync(fd);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_dup2
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      int dup2(int oldfd, int newfd)
 */
static int default_posix1_handler_dup2(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    int oldfd;
    int newfd;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    oldfd = arg0->slot[0];
    newfd = arg1->slot[0];
    rc = dup2(oldfd, newfd);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_lockf
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      int lockf(int fd, int cmd, off_t len)
 */
static int default_posix1_handler_lockf(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    int cmd;
    off_t len;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    cmd = arg1->slot[0];
    len = (int) arg2->slot[0];
    rc = lockf(fd, cmd, len);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_truncate
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      int truncate(const char *path, off_t length)
 */
static int default_posix1_handler_truncate(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *path;
    off_t length;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    path = GET_LS_PTR(arg0->slot[0]);
    length = (int) arg1->slot[0];
    rc = truncate(path, length);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_mkstemp
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      int mkstemp(char *template)
 */
static int default_posix1_handler_mkstemp(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *template;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    template = GET_LS_PTR(arg0->slot[0]);
    rc = mkstemp(template);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_mktemp
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      char *mktemp(char *template)
 */
static int default_posix1_handler_mktemp(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *template;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    template = GET_LS_PTR(arg0->slot[0]);
    mktemp(template);
    /*
     * Note that POSIX says (and glibc implements) that mktemp always
     * returns the address of template, but the linux man page incorrectly
     * says (or said) NULL is returned on error.
     */
    rc = arg0->slot[0];
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_opendir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *       DIR *opendir(const char *dirname);
 */
static int default_posix1_handler_opendir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    char *name;
    addr64 dir64;
    DIR *dir;

    DEBUG_PRINTF("%s\n", __func__);
    name = GET_LS_PTR(arg0->slot[0]);
    dir = opendir(name);
    dir64.all64 = spe_ptrto64(dir);
    PUT_LS_RC(dir64.by32[0], dir64.by32[1], 0, errno);
    return 0;
}

/**
 * default_posix1_handler_closedir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *       DIR *closedir(const char *dirname);
 */
static int default_posix1_handler_closedir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    DIR *dir;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    dir = spe_reg128toptr(arg0);
    rc = closedir(dir);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_readdir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data, plus a secondary argument pass the
 * location of the dirent.
 *
 * Implement:
 *       struct dirent *readdir(DIR *dir)
 */
static int default_posix1_handler_readdir(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    DIR *dir;
    struct dirent *ent;
    struct spe_compat_dirent *spe_ent;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    dir = spe_reg128toptr(arg0);
    spe_ent = GET_LS_PTR(arg1->slot[0]);
    ent = readdir(dir);
    if (ent) {
        spe_copy_dirent(spe_ent, ent);
        rc = arg1->slot[0]; /* LS address of spe_ent */
    } else {
        rc = 0;
    }
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_rewinddir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data
 *
 * Implement:
 *      void rewinddir(DIR *dir)
 */
static int default_posix1_handler_rewinddir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    DIR *dir;

    DEBUG_PRINTF("%s\n", __func__);
    dir = spe_reg128toptr(arg0);
    rewinddir(dir);
    PUT_LS_RC(0, 0, 0, 0);
    return 0;
}

/**
 * default_posix1_handler_seekdir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data
 *
 * Implement:
 *      void seekdir(DIR *dir)
 */
static int default_posix1_handler_seekdir(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    DIR *dir;
    off_t offset;

    DEBUG_PRINTF("%s\n", __func__);
    dir = spe_reg128toptr(arg0);
    offset = (int) arg1->slot[0];
    seekdir(dir, offset);
    PUT_LS_RC(0, 0, 0, 0);
    return 0;
}

/**
 * default_posix1_handler_telldir
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data
 *
 * Implement:
 */
static int default_posix1_handler_telldir(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    DIR *dir;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    dir = spe_reg128toptr(arg0);
    rc = telldir(dir);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_sched_yield
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data
 *
 * Implement:
 *      int sched_yield(void)
 */
static int default_posix1_handler_sched_yield(char *ls, unsigned long opdata)
{
    DECL_0_ARGS();
    DECL_RET();
    /*
     * The kernel will call spu_yield internally, so do nothing here.
     */
    DEBUG_PRINTF("%s\n", __func__);
    PUT_LS_RC(0, 0, 0, 0);
    return 0;
}

/**
 * default_posix1_handler_umask
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data
 *
 * Implement:
 *      mode_t umask(mode_t mask)
 */
static int default_posix1_handler_umask(char *ls, unsigned long opdata)
{
    DECL_1_ARGS();
    DECL_RET();
    mode_t mask;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    mask = arg0->slot[0];
    rc = umask(mask);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_utime
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int utime(const char *filename, const struct utimbuf *buf)
 */
static int default_posix1_handler_utime(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *filename;
    struct utimbuf buf;
    struct spe_compat_utimbuf *spebuf;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    filename = GET_LS_PTR(arg0->slot[0]);
    spebuf = GET_LS_PTR(arg1->slot[0]);
    buf.actime = spebuf->actime;
    buf.modtime = spebuf->modtime;
    rc = utime(filename, &buf);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_utimes
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *     int utimes(const char *filename, const struct timeval times[2])
 */
static int default_posix1_handler_utimes(char *ls, unsigned long opdata)
{
    DECL_2_ARGS();
    DECL_RET();
    char *filename;
    struct spe_compat_timeval *spetimes;
    struct timeval times[2];
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    filename = GET_LS_PTR(arg0->slot[0]);
    spetimes = GET_LS_PTR(arg1->slot[0]);
    times[0].tv_sec = spetimes[0].tv_sec;
    times[0].tv_usec = spetimes[0].tv_usec;
    times[1].tv_sec = spetimes[1].tv_sec;
    times[1].tv_usec = spetimes[1].tv_usec;
    rc = utimes(filename, times);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_pread
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      ssize_t pread(int fd, void *buf, size_t count, off_t offset)
 */
static int default_posix1_handler_pread(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    int fd;
    void *buf;
    size_t count;
    off_t offset;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    buf = GET_LS_PTR(arg1->slot[0]);
    count = arg2->slot[0];
    offset = (int) arg3->slot[0];
    rc = pread(fd, buf, count, offset);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_pwrite
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset)
 */
static int default_posix1_handler_pwrite(char *ls, unsigned long opdata)
{
    DECL_4_ARGS();
    DECL_RET();
    int fd;
    void *buf;
    size_t count;
    off_t offset;
    ssize_t sz;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    buf = GET_LS_PTR(arg1->slot[0]);
    count = arg2->slot[0];
    offset = (int) arg3->slot[0];
    sz = pwrite(fd, buf, count, offset);
    rc = sz;
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_readv
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      ssize_t readv(int fd, const struct iovec *vec, int count)
 */
static int default_posix1_handler_readv(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    struct iovec vec[IOV_MAX];
    struct ls_iovec *lsvec;
    size_t count;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    lsvec = GET_LS_PTR(arg1->slot[0]);
    count = arg2->slot[0];
    rc = check_conv_spuvec(ls, vec, lsvec, count);
    if (!rc)
        rc = readv(fd, vec, count);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

/**
 * default_posix1_handler_writev
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Implement:
 *      ssize_t writev(int fd, const struct iovec *vec, int count)
 */
static int default_posix1_handler_writev(char *ls, unsigned long opdata)
{
    DECL_3_ARGS();
    DECL_RET();
    int fd;
    struct iovec vec[IOV_MAX];
    struct ls_iovec *lsvec;
    size_t count;
    int rc;

    DEBUG_PRINTF("%s\n", __func__);
    fd = arg0->slot[0];
    lsvec = GET_LS_PTR(arg1->slot[0]);
    count = arg2->slot[0];
    rc = check_conv_spuvec(ls, vec, lsvec, count);
    if (!rc)
        rc = writev(fd, vec, count);
    PUT_LS_RC(rc, 0, 0, errno);
    return 0;
}

static int (*default_posix1_funcs[SPE_POSIX1_NR_OPCODES]) (char *, unsigned long) = {
	[SPE_POSIX1_UNUSED]		= NULL,
	[SPE_POSIX1_ADJTIMEX]		= default_posix1_handler_adjtimex,
	[SPE_POSIX1_CLOSE]		= default_posix1_handler_close,
	[SPE_POSIX1_CREAT]		= default_posix1_handler_creat,
	[SPE_POSIX1_FSTAT]		= default_posix1_handler_fstat,
	[SPE_POSIX1_FTOK]		= default_posix1_handler_ftok,
	[SPE_POSIX1_GETPAGESIZE]	= default_posix1_handler_getpagesize,
	[SPE_POSIX1_GETTIMEOFDAY]	= default_posix1_handler_gettimeofday,
	[SPE_POSIX1_KILL]		= default_posix1_handler_kill,
	[SPE_POSIX1_LSEEK]		= default_posix1_handler_lseek,
	[SPE_POSIX1_LSTAT]		= default_posix1_handler_lstat,
	[SPE_POSIX1_MMAP]		= default_posix1_handler_mmap,
	[SPE_POSIX1_MREMAP]		= default_posix1_handler_mremap,
	[SPE_POSIX1_MSYNC]		= default_posix1_handler_msync,
	[SPE_POSIX1_MUNMAP]		= default_posix1_handler_munmap,
	[SPE_POSIX1_OPEN]		= default_posix1_handler_open,
	[SPE_POSIX1_READ]		= default_posix1_handler_read,
	[SPE_POSIX1_SHMAT]		= default_posix1_handler_shmat,
	[SPE_POSIX1_SHMCTL]		= default_posix1_handler_shmctl,
	[SPE_POSIX1_SHMDT]		= default_posix1_handler_shmdt,
	[SPE_POSIX1_SHMGET]		= default_posix1_handler_shmget,
	[SPE_POSIX1_SHM_OPEN]		= default_posix1_handler_shm_open,
	[SPE_POSIX1_SHM_UNLINK]		= default_posix1_handler_shm_unlink,
	[SPE_POSIX1_STAT]		= default_posix1_handler_stat,
	[SPE_POSIX1_UNLINK]		= default_posix1_handler_unlink,
	[SPE_POSIX1_WAIT]		= default_posix1_handler_wait,
	[SPE_POSIX1_WAITPID]		= default_posix1_handler_waitpid,
	[SPE_POSIX1_WRITE]		= default_posix1_handler_write,
	[SPE_POSIX1_FTRUNCATE]		= default_posix1_handler_ftruncate,
	[SPE_POSIX1_ACCESS]		= default_posix1_handler_access,
	[SPE_POSIX1_DUP]		= default_posix1_handler_dup,
	[SPE_POSIX1_TIME]		= default_posix1_handler_time,
	[SPE_POSIX1_NANOSLEEP]		= default_posix1_handler_nanosleep,
	[SPE_POSIX1_CHDIR]		= default_posix1_handler_chdir,
	[SPE_POSIX1_FCHDIR]		= default_posix1_handler_fchdir,
	[SPE_POSIX1_MKDIR]		= default_posix1_handler_mkdir,
	[SPE_POSIX1_MKNOD]		= default_posix1_handler_mknod,
	[SPE_POSIX1_RMDIR]		= default_posix1_handler_rmdir,
	[SPE_POSIX1_CHMOD]		= default_posix1_handler_chmod,
	[SPE_POSIX1_FCHMOD]		= default_posix1_handler_fchmod,
	[SPE_POSIX1_CHOWN]		= default_posix1_handler_chown,
	[SPE_POSIX1_FCHOWN]		= default_posix1_handler_fchown,
	[SPE_POSIX1_LCHOWN]		= default_posix1_handler_lchown,
	[SPE_POSIX1_GETCWD]             = default_posix1_handler_getcwd,
	[SPE_POSIX1_LINK]		= default_posix1_handler_link,
	[SPE_POSIX1_SYMLINK]		= default_posix1_handler_symlink,
	[SPE_POSIX1_READLINK]		= default_posix1_handler_readlink,
	[SPE_POSIX1_SYNC]		= default_posix1_handler_sync,
	[SPE_POSIX1_FSYNC]		= default_posix1_handler_fsync,
	[SPE_POSIX1_FDATASYNC]		= default_posix1_handler_fdatasync,
	[SPE_POSIX1_DUP2]		= default_posix1_handler_dup2,
	[SPE_POSIX1_LOCKF]		= default_posix1_handler_lockf,
	[SPE_POSIX1_TRUNCATE]		= default_posix1_handler_truncate,
	[SPE_POSIX1_MKSTEMP]		= default_posix1_handler_mkstemp,
	[SPE_POSIX1_MKTEMP]		= default_posix1_handler_mktemp,
	[SPE_POSIX1_OPENDIR]		= default_posix1_handler_opendir,
	[SPE_POSIX1_CLOSEDIR]		= default_posix1_handler_closedir,
	[SPE_POSIX1_READDIR]		= default_posix1_handler_readdir,
	[SPE_POSIX1_REWINDDIR]		= default_posix1_handler_rewinddir,
	[SPE_POSIX1_SEEKDIR]		= default_posix1_handler_seekdir,
	[SPE_POSIX1_TELLDIR]		= default_posix1_handler_telldir,
	[SPE_POSIX1_SCHED_YIELD]	= default_posix1_handler_sched_yield,
	[SPE_POSIX1_UMASK]		= default_posix1_handler_umask,
	[SPE_POSIX1_UTIME]		= default_posix1_handler_utime,
	[SPE_POSIX1_UTIMES]		= default_posix1_handler_utimes,
	[SPE_POSIX1_PREAD]		= default_posix1_handler_pread,
	[SPE_POSIX1_PWRITE]		= default_posix1_handler_pwrite,
	[SPE_POSIX1_READV]		= default_posix1_handler_readv,
	[SPE_POSIX1_WRITEV]		= default_posix1_handler_writev,
};

/**
 * default_posix1_handler
 * @ls: base pointer to local store area.
 * @opdata: POSIX.1 call opcode & data.
 *
 * Default POSIX.1 call dispatch function.
 */
int _base_spe_default_posix1_handler(char *base, unsigned long offset)
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

    default_posix1_funcs[op] (base, opdata);

    return 0;
}

