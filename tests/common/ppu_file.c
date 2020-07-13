/*
 *  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 *
 *  Copyright (C) 2008 Sony Computer Entertainment Inc.
 *  Copyright 2008 Sony Corp.
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

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "ppu_libspe2_test.h"


/*** SPUFS direct access ***/
int spufs_open(const spe_context_ptr_t spe, const char *name, int mode)
{
  char node[256];

  sprintf(node, "/spu/spethread-%lu-%lu/%s",
	  (unsigned long)getpid(), (unsigned long)spe, name);
  return open(node, mode);
}

/*** consuming FDs ***/
#define DUMMY_FD_FILE "/dev/null"
static int g_num_dummy_fd = 0;
static int g_dummy_fd[MAX_DUMMY_FDS];

int dummy_file_open(int num)
{
  if (num > MAX_DUMMY_FDS) {
    eprintf("No more room for dummy FDs.\n");
    return -1;
  }

  /* open specified number of FDs. */
  while (g_num_dummy_fd < num) {
    int fd;
    fd = open(DUMMY_FD_FILE, O_RDONLY);
    if (fd == -1) {
      perror("open(" DUMMY_FD_FILE ")");
      return -1;
    }
    g_dummy_fd[g_num_dummy_fd++] = fd;
  }

  return 0;
}

int dummy_file_open_r(int num)
{
  /* open files as many as possible */
  while (1) {
    int fd;
    fd = open(DUMMY_FD_FILE, O_RDONLY);
    if (fd == -1) {
      if (errno == EMFILE) {
	break;
      }
      else {
	perror("open(" DUMMY_FD_FILE ")");
	return -1;
      }
    }
    if (g_num_dummy_fd >= MAX_DUMMY_FDS) {
      close(fd);
      eprintf("No more room for dummy FDs.\n");
      return -1;
    }
    g_dummy_fd[g_num_dummy_fd++] = fd;
  }

  /* close specified number of FDs. */
  while (num-- > 0) {
    g_num_dummy_fd--;

    close(g_dummy_fd[g_num_dummy_fd]);
  }

  return g_num_dummy_fd;
}

/* close all dummy files */
void dummy_file_close(void)
{
  int i;
  for (i = 0; i < g_num_dummy_fd; i++) {
    close(g_dummy_fd[i]);
  }
  g_num_dummy_fd = 0;
}

int tmpfile_open(char *name, const char *base)
{
  static int serial = 0;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  pthread_mutex_lock(&lock);
  sprintf(name, "%s-%lu.%u.tmp", base, (unsigned long)getpid(), serial++);
  pthread_mutex_unlock(&lock);

  return open(name, O_RDWR | O_CREAT | O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}
