/*
 * libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/spu.h>
#include <sys/stat.h>
#include <unistd.h>

#include "create.h"
#include "spebase.h"

struct fd_attr
{
	const char *name;
	int mode;
};

static const struct fd_attr spe_fd_attr[NUM_MBOX_FDS] = {
	[FD_MBOX]	= { .name = "mbox",      .mode = O_RDONLY, },
	[FD_MBOX_STAT]	= { .name = "mbox_stat", .mode = O_RDONLY, },
	[FD_IBOX]	= { .name = "ibox",      .mode = O_RDONLY, },
	[FD_IBOX_NB]	= { .name = "ibox",      .mode = O_RDONLY | O_NONBLOCK, },
	[FD_IBOX_STAT]	= { .name = "ibox_stat", .mode = O_RDONLY, },
	[FD_WBOX]	= { .name = "wbox",      .mode = O_WRONLY, },
	[FD_WBOX_NB]	= { .name = "wbox",      .mode = O_WRONLY | O_NONBLOCK, },
	[FD_WBOX_STAT]	= { .name = "wbox_stat", .mode = O_RDONLY, },
	[FD_SIG1]	= { .name = "signal1",   .mode = O_RDWR, },
	[FD_SIG2]	= { .name = "signal2",   .mode = O_RDWR, },
	[FD_MFC]	= { .name = "mfc",       .mode = O_RDWR, },
	[FD_MSS]	= { .name = "mss",       .mode = O_RDWR, },
};

void *mapfileat( int dir, const char* filename, int size)
{   
	int fd_temp;
	void *ret;
	
	fd_temp=openat(dir, filename, O_RDWR);
	if (fd_temp < 0) {
		DEBUG_PRINTF ("ERROR: Could not open SPE %s file.\n",filename);
		errno=EFAULT;
		return (void *)-1;
	}
	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_temp, 0);
	close(fd_temp);

	return ret;
}

int setsignotify(int dir, const char* filename)
{
	int fd_sig;
	char one[] = "1";
	int ignore;
	fd_sig = openat (dir, filename, O_RDWR);
	if (fd_sig < 0) {
		return -1; 
	}
	ignore=write (fd_sig, one, 1);
	close(fd_sig);
	return 0;
}

void _base_spe_context_lock(spe_context_ptr_t spe, enum fd_name fdesc)
{
	pthread_mutex_lock(&spe->base_private->fd_lock[fdesc]);
}

void _base_spe_context_unlock(spe_context_ptr_t spe, enum fd_name fdesc)
{
	pthread_mutex_unlock(&spe->base_private->fd_lock[fdesc]);
}

int open_if_closed(struct spe_context *spe, enum fd_name fdesc, int locked)
{
	if (!locked) _base_spe_context_lock(spe, fdesc);
	
	if (spe->base_private->spe_fds_array[(int)fdesc] != -1) { // already open
		spe->base_private->spe_fds_refcount[(int)fdesc]++;
	}
	else {
		spe->base_private->spe_fds_array[(int)fdesc] =
			openat(spe->base_private->fd_spe_dir, spe_fd_attr[fdesc].name, spe_fd_attr[fdesc].mode);
		if (spe->base_private->spe_fds_array[(int)fdesc] > 0)
			spe->base_private->spe_fds_refcount[(int)fdesc]++;
	}

	if (!locked) _base_spe_context_unlock(spe, fdesc);
	
	return spe->base_private->spe_fds_array[(int)fdesc];
}

void close_if_open(struct spe_context *spe, enum fd_name fdesc)
{
	_base_spe_context_lock(spe, fdesc);

	if (spe->base_private->spe_fds_array[(int)fdesc] != -1 &&
		spe->base_private->spe_fds_refcount[(int)fdesc] == 1){
		
		spe->base_private->spe_fds_refcount[(int)fdesc]--;
		close(spe->base_private->spe_fds_array[(int)fdesc]);
		
		spe->base_private->spe_fds_array[(int)fdesc] = -1;
	}else if (spe->base_private->spe_fds_refcount[(int)fdesc] > 0) {
		spe->base_private->spe_fds_refcount[(int)fdesc]--;
	}
		
	_base_spe_context_unlock(spe, fdesc);
}

spe_context_ptr_t _base_spe_context_create(unsigned int flags, spe_gang_context_ptr_t gctx, spe_context_ptr_t aff_spe)
{
	char pathname[256];
	int i;
	unsigned int spu_createflags = 0;

	/* Put some sane defaults into the SPE context
	 */
	 
	struct spe_context *spe = NULL;
	
	spe = calloc (1, sizeof *spe);
	if (!spe) {
		DEBUG_PRINTF ("ERROR: Could not allocate spe context.\n");
		errno = ENOMEM;
		return NULL;
	}
	
	spe->base_private = calloc (1, sizeof *spe->base_private);
	if (!spe->base_private) {
		DEBUG_PRINTF ("ERROR: Could not allocate spe->base_private context.\n");
		errno = ENOMEM;
		return NULL;
	}
	
	spe->base_private->mem_mmap_base = (void*) -1;
	spe->base_private->psmap_mmap_base = (void*) -1;
	spe->base_private->mssync_mmap_base = (void*) -1;
	spe->base_private->mfc_mmap_base = (void*) -1;
	spe->base_private->cntl_mmap_base = (void*) -1;
	spe->base_private->signal1_mmap_base = (void*) -1;
	spe->base_private->signal2_mmap_base = (void*) -1;
	
	for ( i=0;i<NUM_MBOX_FDS;i++){
		spe->base_private->spe_fds_array[i]=-1;
		pthread_mutex_init(&spe->base_private->fd_lock[i], NULL);
	}	

	/* Make SPE-Create Flags
	 */

 	if ( flags & SPE_ISOLATE ) {
		flags |= SPE_MAP_PS;
 		spu_createflags |= SPU_CREATE_ISOLATE | SPU_CREATE_NOSCHED;
	}
 	
  	if ( flags & SPE_EVENTS_ENABLE )
  		spu_createflags |= SPU_CREATE_EVENTS_ENABLED;
  		
  	if ( aff_spe )
		spu_createflags |= SPU_CREATE_AFFINITY_SPU;

	if ( flags & SPE_AFFINITY_MEMORY )
		spu_createflags |= SPU_CREATE_AFFINITY_MEM;
 	
	/* Make the SPUFS directory for the SPE
	 */
	
	if (gctx == NULL)
		sprintf (pathname, "/spu/spethread-%i-%lu",
			getpid (), (unsigned long)spe);
	else
		sprintf (pathname, "/spu/%s/spethread-%i-%lu",
			gctx->base_private->gangname, getpid (), (unsigned long)spe);
		
	if ( aff_spe )
		spe->base_private->fd_spe_dir = spu_create(pathname, spu_createflags, S_IRUSR | S_IWUSR | S_IXUSR,
										aff_spe->base_private->fd_spe_dir);
	else
		spe->base_private->fd_spe_dir = spu_create(pathname, spu_createflags, S_IRUSR | S_IWUSR | S_IXUSR);
	if (spe->base_private->fd_spe_dir < 0) {
		DEBUG_PRINTF ("ERROR: Could not create SPE %s\n", pathname);
		perror("spu_create()");
		errno=EFAULT;
		return NULL;
	}
	
	spe->base_private->flags=flags;
	
	/* Map the required areas into process memory
	 */

	spe->base_private->mem_mmap_base = mapfileat( spe->base_private->fd_spe_dir, "mem", LS_SIZE);
	if ( spe->base_private->mem_mmap_base == MAP_FAILED ) {
		DEBUG_PRINTF ("ERROR: Could not map SPE memory. \n");
		errno = ENOMEM;
		return NULL;
	}

	if ( flags & SPE_MAP_PS ){

		spe->base_private->psmap_mmap_base =  mapfileat( spe->base_private->fd_spe_dir, "psmap", PSMAP_SIZE);
		
		if (spe->base_private->psmap_mmap_base == MAP_FAILED) {
			spe->base_private->mfc_mmap_base =  mapfileat( spe->base_private->fd_spe_dir, "mfc", MFC_SIZE);
			spe->base_private->mssync_mmap_base =  mapfileat( spe->base_private->fd_spe_dir, "mss", MSS_SIZE);
			spe->base_private->cntl_mmap_base =  mapfileat( spe->base_private->fd_spe_dir, "cntl", CNTL_SIZE);
			spe->base_private->signal1_mmap_base =  mapfileat( spe->base_private->fd_spe_dir, "signal1",SIGNAL_SIZE);
			spe->base_private->signal2_mmap_base =  mapfileat( spe->base_private->fd_spe_dir, "signal2", SIGNAL_SIZE);
			if ( spe->base_private->mfc_mmap_base == MAP_FAILED || 
		     	spe->base_private->cntl_mmap_base == MAP_FAILED ||
		     	spe->base_private->signal1_mmap_base == MAP_FAILED || 
		     	spe->base_private->signal2_mmap_base == MAP_FAILED ) {
				DEBUG_PRINTF ("ERROR: Could not map SPE PS memory. \n");
				errno = ENOMEM;
				return NULL;
			}
		} else {
			spe->base_private->mssync_mmap_base = spe->base_private->psmap_mmap_base + 0x00000;	
			spe->base_private->mfc_mmap_base = spe->base_private->psmap_mmap_base + 0x03000;	
			spe->base_private->cntl_mmap_base = spe->base_private->psmap_mmap_base + 0x04000;	
			spe->base_private->signal1_mmap_base = spe->base_private->psmap_mmap_base + 0x14000;	
			spe->base_private->signal2_mmap_base = spe->base_private->psmap_mmap_base + 0x1c000;	
		}
	}
	
	if ( flags & SPE_CFG_SIGNOTIFY1_OR ) {
		if (setsignotify(spe->base_private->fd_spe_dir, "signal1_type")) {
			DEBUG_PRINTF ("ERROR: Could not open SPE signal1_type file.\n");
			errno = EFAULT;
			return NULL;
		}
	}

	if ( flags & SPE_CFG_SIGNOTIFY2_OR ) {
		if (setsignotify(spe->base_private->fd_spe_dir, "signal2_type")) {
			DEBUG_PRINTF ("ERROR: Could not open SPE signal2_type file.\n");
			errno = EFAULT;
			return NULL;
		}
	}

	
	return spe;
}

spe_gang_context_ptr_t _base_spe_gang_context_create(unsigned int flags)
{
	char pathname[256];
	struct spe_gang_context_base_priv *pgctx = NULL;
	struct spe_gang_context *gctx = NULL;
	
	gctx = calloc (1, sizeof *gctx);
	if (!gctx) {
		DEBUG_PRINTF ("ERROR: Could not allocate spe context.\n");
		errno = ENOMEM;
		return NULL;
	}

	pgctx = calloc (1, sizeof *pgctx);
	if (!pgctx) {
		DEBUG_PRINTF ("ERROR: Could not allocate spe context.\n");
		errno = ENOMEM;
		return NULL;
	}
	
	gctx->base_private = pgctx;
	
	sprintf (gctx->base_private->gangname, "gang-%i-%lu", getpid(), (unsigned long) gctx);
	sprintf (pathname, "/spu/%s", gctx->base_private->gangname);

	gctx->base_private->fd_gang_dir = spu_create(pathname, 2, S_IRUSR | S_IWUSR | S_IXUSR);
	if (gctx->base_private->fd_gang_dir < 0) {
		DEBUG_PRINTF ("ERROR: Could not create Gang %s\n", pathname);
		errno=EFAULT;
		return NULL;
	}
	
	gctx->base_private->flags = flags;
	
	return gctx;
}

int _base_spe_context_destroy(spe_context_ptr_t spe)
{
	int i;
	
	if(spe->base_private->psmap_mmap_base != (void*)-1) {
		munmap(spe->base_private->psmap_mmap_base, PSMAP_SIZE);
	}
	else {
		if(spe->base_private->mfc_mmap_base != (void*)-1)
			munmap(spe->base_private->mfc_mmap_base, MFC_SIZE);
		if(spe->base_private->mssync_mmap_base != (void*)-1)
			munmap(spe->base_private->mssync_mmap_base, MSS_SIZE);
		if(spe->base_private->cntl_mmap_base != (void*)-1)
			munmap(spe->base_private->cntl_mmap_base, CNTL_SIZE);
		if(spe->base_private->signal1_mmap_base != (void*)-1)
			munmap(spe->base_private->signal1_mmap_base,SIGNAL_SIZE);
		if(spe->base_private->signal2_mmap_base != (void*)-1)
			munmap(spe->base_private->signal2_mmap_base, SIGNAL_SIZE);
	}

	if(spe->base_private->mem_mmap_base != (void*)-1)
		munmap(spe->base_private->mem_mmap_base, LS_SIZE);

	for ( i=0;i<NUM_MBOX_FDS;i++){
		close(spe->base_private->spe_fds_array[i]);
		pthread_mutex_destroy(&spe->base_private->fd_lock[i]);
	}
	
	close(spe->base_private->fd_spe_dir);
	
	free(spe->base_private);
	free(spe);

	__spe_context_update_event();

	return 0;
}

int _base_spe_gang_context_destroy(spe_gang_context_ptr_t gctx)
{
	close(gctx->base_private->fd_gang_dir);
	free(gctx->base_private);
	free(gctx);
	return 0;
}

