/* default_posix1_handler.h - emulate SPE C99 library calls.
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

#ifndef __DEFAULT_POSIX1_HANDLER_H__
#define __DEFAULT_POSIX1_HANDLER_H__

#define SPE_POSIX1_CLASS     0x2101

extern int default_posix1_handler(char *ls, unsigned long args);
extern int default_posix1_handler_adjtimex(char *ls, unsigned long args);
extern int default_posix1_handler_close(char *ls, unsigned long args);
extern int default_posix1_handler_creat(char *ls, unsigned long args);
extern int default_posix1_handler_fstat(char *ls, unsigned long args);
extern int default_posix1_handler_ftok(char *ls, unsigned long args);
extern int default_posix1_handler_getpagesize(char *ls, unsigned long args);
extern int default_posix1_handler_gettimeofday(char *ls, unsigned long args);
extern int default_posix1_handler_kill(char *ls, unsigned long args);
extern int default_posix1_handler_lseek(char *ls, unsigned long args);
extern int default_posix1_handler_lstat(char *ls, unsigned long args);
extern int default_posix1_handler_mmap(char *ls, unsigned long args);
extern int default_posix1_handler_mremap(char *ls, unsigned long args);
extern int default_posix1_handler_msync(char *ls, unsigned long args);
extern int default_posix1_handler_munmap(char *ls, unsigned long args);
extern int default_posix1_handler_open(char *ls, unsigned long args);
extern int default_posix1_handler_read(char *ls, unsigned long args);
extern int default_posix1_handler_shmat(char *ls, unsigned long args);
extern int default_posix1_handler_shmctl(char *ls, unsigned long args);
extern int default_posix1_handler_shmdt(char *ls, unsigned long args);
extern int default_posix1_handler_shmget(char *ls, unsigned long args);
extern int default_posix1_handler_shm_open(char *ls, unsigned long args);
extern int default_posix1_handler_shm_unlink(char *ls, unsigned long args);
extern int default_posix1_handler_stat(char *ls, unsigned long args);
extern int default_posix1_handler_unlink(char *ls, unsigned long args);
extern int default_posix1_handler_wait(char *ls, unsigned long args);
extern int default_posix1_handler_waitpid(char *ls, unsigned long args);
extern int default_posix1_handler_write(char *ls, unsigned long args);
extern int default_posix1_handler_ftruncate(char *ls, unsigned long args);
extern int default_posix1_handler_access(char *ls, unsigned long args);
extern int default_posix1_handler_dup(char *ls, unsigned long args);
extern int default_posix1_handler_time(char *ls, unsigned long args);

#endif /* __DEFAULT_POSIX1_HANDLER_H__ */
