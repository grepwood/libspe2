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
#include "spebase.h"
#include "create.h"

#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

 /*
 * accessor functions for private members 
 */

void* _base_spe_ps_area_get(spe_context_ptr_t spe, enum ps_area area)
{
	void *ptr;

	switch (area) {
		case SPE_MSSYNC_AREA:
			ptr = spe->base_private->mssync_mmap_base;
			break;
		case SPE_MFC_COMMAND_AREA:
			ptr = spe->base_private->mfc_mmap_base;
			break;
		case SPE_CONTROL_AREA:
			ptr = spe->base_private->cntl_mmap_base;
			break;
		case SPE_SIG_NOTIFY_1_AREA:
			ptr = spe->base_private->signal1_mmap_base;
			break;
		case SPE_SIG_NOTIFY_2_AREA:
			ptr = spe->base_private->signal2_mmap_base;
			break;
		default:
			errno = EINVAL;
			return NULL;
			break;
	}

	if (ptr == MAP_FAILED) {
		errno = EACCES;
		return NULL;
	}

	return ptr;
}

void* _base_spe_ls_area_get(spe_context_ptr_t spe)
{
	return spe->base_private->mem_mmap_base;
}

__attribute__ ((noinline)) void  __spe_context_update_event(void)
{
	return;
}

int __base_spe_event_source_acquire(spe_context_ptr_t spe, enum fd_name fdesc)
{
	return _base_spe_open_if_closed(spe, fdesc, 0);
}

void __base_spe_event_source_release(struct spe_context *spe, enum fd_name fdesc)
{
	_base_spe_close_if_open(spe, fdesc);
}

int __base_spe_spe_dir_get(spe_context_ptr_t spe)
{
	return spe->base_private->fd_spe_dir;
}

/**
 * speevent users read from this end
 */
int __base_spe_stop_event_source_get(spe_context_ptr_t spe)
{
	return spe->base_private->ev_pipe[1];
}

/**
 * speevent writes to this end
 */
int __base_spe_stop_event_target_get(spe_context_ptr_t spe)
{
	return spe->base_private->ev_pipe[0];
}

int _base_spe_ls_size_get(spe_context_ptr_t spe)
{
	return LS_SIZE;
}
