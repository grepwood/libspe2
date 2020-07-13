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


struct fd_attr {
	const char *name;
	int mode;
};

static const struct fd_attr spe_fd_attr[NUM_MBOX_FDS] = {
	[FD_MBOX]	= { .name = "mbox",      .mode = O_RDONLY },
	[FD_MBOX_STAT]	= { .name = "mbox_stat", .mode = O_RDONLY },
	[FD_IBOX]	= { .name = "ibox",      .mode = O_RDONLY },
	[FD_IBOX_NB]	= { .name = "ibox",      .mode = O_RDONLY |O_NONBLOCK },
	[FD_IBOX_STAT]	= { .name = "ibox_stat", .mode = O_RDONLY },
	[FD_WBOX]	= { .name = "wbox",      .mode = O_WRONLY },
	[FD_WBOX_NB]	= { .name = "wbox",      .mode = O_WRONLY|O_NONBLOCK },
	[FD_WBOX_STAT]	= { .name = "wbox_stat", .mode = O_RDONLY },
	[FD_SIG1]	= { .name = "signal1",   .mode = O_WRONLY },
	[FD_SIG2]	= { .name = "signal2",   .mode = O_WRONLY },
	[FD_MFC]	= { .name = "mfc",       .mode = O_RDWR },
	[FD_MSS]	= { .name = "mss",       .mode = O_RDWR },
};

static void *mapfileat(int dir, const char *filename, int size)
{
	int fd_temp;
	void *ret;

	fd_temp = openat(dir, filename, O_RDWR);
	if (fd_temp < 0) {
		DEBUG_PRINTF("ERROR: Could not open SPE %s file.\n", filename);
		errno = EFAULT;
		return MAP_FAILED;
	}
	ret = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_temp, 0);
	close(fd_temp);

	return ret;
}

static int setsignotify(int dir, const char *filename)
{
	int fd_sig, rc = 0;
	char one = '1';

	fd_sig = openat(dir, filename, O_RDWR);
	if (fd_sig < 0)
		return -1;

	if (write(fd_sig, &one, sizeof(one)) != sizeof(one))
		rc = -1;

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

int _base_spe_open_if_closed(struct spe_context *spe, enum fd_name fdesc, int locked)
{
	if (!locked)
		_base_spe_context_lock(spe, fdesc);

	/* already open? */
	if (spe->base_private->spe_fds_array[fdesc] != -1) {
		spe->base_private->spe_fds_refcount[fdesc]++;
	} else {
		spe->base_private->spe_fds_array[fdesc] =
			openat(spe->base_private->fd_spe_dir,
					spe_fd_attr[fdesc].name,
					spe_fd_attr[fdesc].mode);

		if (spe->base_private->spe_fds_array[(int)fdesc] > 0)
			spe->base_private->spe_fds_refcount[(int)fdesc]++;
	}

	if (!locked)
		_base_spe_context_unlock(spe, fdesc);

	return spe->base_private->spe_fds_array[(int)fdesc];
}

void _base_spe_close_if_open(struct spe_context *spe, enum fd_name fdesc)
{
	_base_spe_context_lock(spe, fdesc);

	if (spe->base_private->spe_fds_array[(int)fdesc] != -1 &&
		spe->base_private->spe_fds_refcount[(int)fdesc] == 1) {

		spe->base_private->spe_fds_refcount[(int)fdesc]--;
		close(spe->base_private->spe_fds_array[(int)fdesc]);

		spe->base_private->spe_fds_array[(int)fdesc] = -1;
	} else if (spe->base_private->spe_fds_refcount[(int)fdesc] > 0) {
		spe->base_private->spe_fds_refcount[(int)fdesc]--;
	}

	_base_spe_context_unlock(spe, fdesc);
}

static int free_spe_context(struct spe_context *spe)
{
	int i;

	if (spe->base_private->psmap_mmap_base != MAP_FAILED) {
		munmap(spe->base_private->psmap_mmap_base, PSMAP_SIZE);

	} else {
		if (spe->base_private->mfc_mmap_base != MAP_FAILED)
			munmap(spe->base_private->mfc_mmap_base, MFC_SIZE);
		if (spe->base_private->mssync_mmap_base != MAP_FAILED)
			munmap(spe->base_private->mssync_mmap_base, MSS_SIZE);
		if (spe->base_private->cntl_mmap_base != MAP_FAILED)
			munmap(spe->base_private->cntl_mmap_base, CNTL_SIZE);
		if (spe->base_private->signal1_mmap_base != MAP_FAILED)
			munmap(spe->base_private->signal1_mmap_base,
					SIGNAL_SIZE);
		if (spe->base_private->signal2_mmap_base != MAP_FAILED)
			munmap(spe->base_private->signal2_mmap_base,
					SIGNAL_SIZE);
	}

	if (spe->base_private->mem_mmap_base != MAP_FAILED)
		munmap(spe->base_private->mem_mmap_base, LS_SIZE);

	for (i = 0; i < NUM_MBOX_FDS; i++) {
		if (spe->base_private->spe_fds_array[i] >= 0)
			close(spe->base_private->spe_fds_array[i]);
		pthread_mutex_destroy(&spe->base_private->fd_lock[i]);
	}

	if (spe->base_private->fd_spe_dir >= 0)
		close(spe->base_private->fd_spe_dir);

	free(spe->base_private);
	free(spe);

	return 0;
}

spe_context_ptr_t _base_spe_context_create(unsigned int flags,
		spe_gang_context_ptr_t gctx, spe_context_ptr_t aff_spe)
{
	char pathname[256];
	int i, aff_spe_fd = 0;
	unsigned int spu_createflags = 0;
	struct spe_context *spe = NULL;
	struct spe_context_base_priv *priv;

	/* We need a loader present to run in emulated isolated mode */
	if (flags & SPE_ISOLATE_EMULATE
			&& !_base_spe_emulated_loader_present()) {
		errno = EINVAL;
		return NULL;
	}

	/* Put some sane defaults into the SPE context */
	spe = malloc(sizeof(*spe));
	if (!spe) {
		DEBUG_PRINTF("ERROR: Could not allocate spe context.\n");
		return NULL;
	}
	memset(spe, 0, sizeof(*spe));

	spe->base_private = malloc(sizeof(*spe->base_private));
	if (!spe->base_private) {
		DEBUG_PRINTF("ERROR: Could not allocate "
				"spe->base_private context.\n");
		free(spe);
		return NULL;
	}

	/* just a convenience variable */
	priv = spe->base_private;

	priv->fd_spe_dir = -1;
	priv->mem_mmap_base = MAP_FAILED;
	priv->psmap_mmap_base = MAP_FAILED;
	priv->mssync_mmap_base = MAP_FAILED;
	priv->mfc_mmap_base = MAP_FAILED;
	priv->cntl_mmap_base = MAP_FAILED;
	priv->signal1_mmap_base = MAP_FAILED;
	priv->signal2_mmap_base = MAP_FAILED;
	priv->loaded_program = NULL;

	for (i = 0; i < NUM_MBOX_FDS; i++) {
		priv->spe_fds_array[i] = -1;
		pthread_mutex_init(&priv->fd_lock[i], NULL);
	}

	/* initialise spu_createflags */
	if (flags & SPE_NOSCHED)
		spu_createflags |=  SPU_CREATE_NOSCHED;

	if (flags & SPE_ISOLATE) {
		flags |= SPE_MAP_PS;
		spu_createflags |= SPU_CREATE_ISOLATE | SPU_CREATE_NOSCHED;
	}

	if (flags & SPE_EVENTS_ENABLE)
		spu_createflags |= SPU_CREATE_EVENTS_ENABLED;

	if (aff_spe)
		spu_createflags |= SPU_CREATE_AFFINITY_SPU;

	if (flags & SPE_AFFINITY_MEMORY)
		spu_createflags |= SPU_CREATE_AFFINITY_MEM;

	/* Make the SPUFS directory for the SPE */
	if (gctx == NULL)
		sprintf(pathname, "/spu/spethread-%i-%lu",
			getpid(), (unsigned long)spe);
	else
		sprintf(pathname, "/spu/%s/spethread-%i-%lu",
			gctx->base_private->gangname, getpid(),
			(unsigned long)spe);

	if (aff_spe)
		aff_spe_fd = aff_spe->base_private->fd_spe_dir;

	priv->fd_spe_dir = spu_create(pathname, spu_createflags,
			S_IRUSR | S_IWUSR | S_IXUSR, aff_spe_fd);

	if (priv->fd_spe_dir < 0) {
		int errno_saved = errno; /* save errno to prevent being overwritten */
		DEBUG_PRINTF("ERROR: Could not create SPE %s\n", pathname);
		perror("spu_create()");
		free_spe_context(spe);
		/* we mask most errors, but leave ENODEV, etc */
		switch (errno_saved) {
		case ENOTSUP:
		case EEXIST:
		case EINVAL:
		case EBUSY:
		case ENODEV:
			errno = errno_saved; /* restore errno */
			break;
		case EPERM:
			/* for ISOLATED mode EPERM signals that this mode is not supported */
			if (flags & SPE_ISOLATE)
				errno = ENODEV;
			else
				errno = errno_saved;
			break;
		default:
			errno = EFAULT;
			break;
		}
		return NULL;
	}

	priv->flags = flags;

	/* Map the required areas into process memory */
	priv->mem_mmap_base = mapfileat(priv->fd_spe_dir, "mem", LS_SIZE);
	if (priv->mem_mmap_base == MAP_FAILED) {
		DEBUG_PRINTF("ERROR: Could not map SPE memory.\n");
		free_spe_context(spe);
		errno = ENOMEM;
		return NULL;
	}

	if (flags & SPE_MAP_PS) {
		/* It's possible to map the entire problem state area with
		 * one mmap - try this first */
		priv->psmap_mmap_base =  mapfileat(priv->fd_spe_dir,
				"psmap", PSMAP_SIZE);

		if (priv->psmap_mmap_base != MAP_FAILED) {
			priv->mssync_mmap_base =
				priv->psmap_mmap_base + MSSYNC_OFFSET;
			priv->mfc_mmap_base =
				priv->psmap_mmap_base + MFC_OFFSET;
			priv->cntl_mmap_base =
				priv->psmap_mmap_base + CNTL_OFFSET;
			priv->signal1_mmap_base =
				priv->psmap_mmap_base + SIGNAL1_OFFSET;
			priv->signal2_mmap_base =
				priv->psmap_mmap_base + SIGNAL2_OFFSET;

		} else {
			/* map each region separately */
			priv->mfc_mmap_base =
				mapfileat(priv->fd_spe_dir, "mfc", MFC_SIZE);
			priv->mssync_mmap_base =
				mapfileat(priv->fd_spe_dir, "mss", MSS_SIZE);
			priv->cntl_mmap_base =
				mapfileat(priv->fd_spe_dir, "cntl", CNTL_SIZE);
			priv->signal1_mmap_base =
				mapfileat(priv->fd_spe_dir, "signal1",
						SIGNAL_SIZE);
			priv->signal2_mmap_base =
				mapfileat(priv->fd_spe_dir, "signal2",
						SIGNAL_SIZE);

			if (priv->mfc_mmap_base == MAP_FAILED ||
					priv->cntl_mmap_base == MAP_FAILED ||
					priv->signal1_mmap_base == MAP_FAILED ||
					priv->signal2_mmap_base == MAP_FAILED) {
				DEBUG_PRINTF("ERROR: Could not map SPE "
						"PS memory.\n");
				free_spe_context(spe);
				errno = ENOMEM;
				return NULL;
			}
		}
	}

	if (flags & SPE_CFG_SIGNOTIFY1_OR) {
		if (setsignotify(priv->fd_spe_dir, "signal1_type")) {
			DEBUG_PRINTF("ERROR: Could not open SPE "
					"signal1_type file.\n");
			free_spe_context(spe);
			errno = EFAULT;
			return NULL;
		}
	}

	if (flags & SPE_CFG_SIGNOTIFY2_OR) {
		if (setsignotify(priv->fd_spe_dir, "signal2_type")) {
			DEBUG_PRINTF("ERROR: Could not open SPE "
					"signal2_type file.\n");
			free_spe_context(spe);
			errno = EFAULT;
			return NULL;
		}
	}

	return spe;
}

static int free_spe_gang_context(struct spe_gang_context *gctx)
{
	if (gctx->base_private->fd_gang_dir >= 0)
		close(gctx->base_private->fd_gang_dir);

	free(gctx->base_private);
	free(gctx);

	return 0;
}

spe_gang_context_ptr_t _base_spe_gang_context_create(unsigned int flags)
{
	char pathname[256];
	struct spe_gang_context_base_priv *pgctx = NULL;
	struct spe_gang_context *gctx = NULL;

	gctx = malloc(sizeof(*gctx));
	if (!gctx) {
		DEBUG_PRINTF("ERROR: Could not allocate spe context.\n");
		return NULL;
	}
	memset(gctx, 0, sizeof(*gctx));

	pgctx = malloc(sizeof(*pgctx));
	if (!pgctx) {
		DEBUG_PRINTF("ERROR: Could not allocate spe context.\n");
		free(gctx);
		return NULL;
	}
	memset(pgctx, 0, sizeof(*pgctx));

	gctx->base_private = pgctx;

	sprintf(gctx->base_private->gangname, "gang-%i-%lu", getpid(),
			(unsigned long)gctx);
	sprintf(pathname, "/spu/%s", gctx->base_private->gangname);

	gctx->base_private->fd_gang_dir = spu_create(pathname, SPU_CREATE_GANG,
				S_IRUSR | S_IWUSR | S_IXUSR);

	if (gctx->base_private->fd_gang_dir < 0) {
		DEBUG_PRINTF("ERROR: Could not create Gang %s\n", pathname);
		free_spe_gang_context(gctx);
		errno = EFAULT;
		return NULL;
	}

	gctx->base_private->flags = flags;

	return gctx;
}

int _base_spe_context_destroy(spe_context_ptr_t spe)
{
	int ret = free_spe_context(spe);

	__spe_context_update_event();

	return ret;
}

int _base_spe_gang_context_destroy(spe_gang_context_ptr_t gctx)
{
	return free_spe_gang_context(gctx);
}

