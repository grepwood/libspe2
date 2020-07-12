#ifndef __SYS_SPU_H
#define __SYS_SPU_H

#include <sys/syscall.h>
#include <unistd.h>
#include <stdarg.h>

#ifndef __NR_spu_run
#define __NR_spu_run 278
#endif

#ifndef __NR_spu_create
#define __NR_spu_create 279
#endif

#define SPU_CREATE_EVENTS_ENABLED 0x0001
#define SPU_CREATE_GANG 0x0002

#define SPU_CREATE_NOSCHED 0x0004
#define SPU_CREATE_ISOLATE 0x0008

#define SPU_CREATE_AFFINITY_SPU                0x0010
#define SPU_CREATE_AFFINITY_MEM                0x0020

static inline int spu_run(int fd, unsigned int *npc, unsigned int *status)
{
	return syscall(__NR_spu_run, fd, npc, status);
}

static int spu_create(const char * path, int flags, mode_t mode, ...)
{
	va_list vargs;
	int fd;
	if (flags & SPU_CREATE_AFFINITY_SPU) {
		va_start(vargs, mode);
		fd = va_arg(vargs, int);
		va_end(vargs);
		return syscall(__NR_spu_create, path, flags, mode, fd);
	}
	else {
		return syscall(__NR_spu_create, path, flags, mode);
	}
}

#endif
