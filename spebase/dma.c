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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/poll.h>

#include "create.h"
#include "dma.h"

static int spe_read_tag_status_block(spe_context_ptr_t spectx, unsigned int *tag_status);
static int spe_read_tag_status_noblock(spe_context_ptr_t spectx, unsigned int *tag_status);

static int spe_do_mfc_put(spe_context_ptr_t spectx, unsigned src, void *dst,
			  unsigned size, unsigned tag, unsigned class,
			  enum mfc_cmd cmd)
{
	struct mfc_command_parameter_area parm = {
		.lsa   = src,
		.ea    = (unsigned long) dst,
		.size  = size,
		.tag   = tag,
		.class = class,
		.cmd   = cmd,
	};
	int ret, fd;

	DEBUG_PRINTF("queuing DMA %x %lx %x %x %x %x\n", parm.lsa,
		parm.ea, parm.size, parm.tag, parm.class, parm.cmd);
#if 0 // fixme
	if (spectx->base_private->flags & SPE_MAP_PS) {
		return 0;
	}
#endif
	DEBUG_PRINTF("%s $d\n", __FUNCTION__, __LINE__);
	fd = open_if_closed(spectx, FD_MFC, 0);
	if (fd != -1) {
		ret = write(fd, &parm, sizeof (parm));
		if ((ret < 0) && (errno != EIO)) {
			perror("spe_do_mfc_put: internal error");
		}
		return ret < 0 ? -1 : 0;
	}
	/* the kernel does not support DMA, so just copy directly */
	memcpy(dst, spectx->base_private->mem_mmap_base + src, size);
	return 0;
}

static int spe_do_mfc_get(spe_context_ptr_t spectx, unsigned int dst, void *src,
			  unsigned int size, unsigned int tag, unsigned int class,
			  enum mfc_cmd cmd)
{
	struct mfc_command_parameter_area parm = {
		.lsa   = dst,
		.ea    = (unsigned long) src,
		.size  = size,
		.tag   = tag,
		.class = class,
		.cmd   = cmd,
	};
	int ret, fd;

	DEBUG_PRINTF("queuing DMA %x %lx %x %x %x %x\n", parm.lsa,
		parm.ea, parm.size, parm.tag, parm.class, parm.cmd);
#if 0 // fixme
	if (spectx->base_private->flags & SPE_MAP_PS) {
		return 0;
	}
#endif
	DEBUG_PRINTF("%s $d\n", __FUNCTION__, __LINE__);
	fd = open_if_closed(spectx, FD_MFC, 0);
	if (fd != -1) {
		ret = write(fd, &parm, sizeof (parm));
		if ((ret < 0) && (errno != EIO)) {
			perror("spe_do_mfc_get: internal error");
		}
		return ret < 0 ? -1 : 0;
	}

	/* the kernel does not support DMA, so just copy directly */
	memcpy(spectx->base_private->mem_mmap_base + dst, src, size);
	return 0;
}

int _base_spe_mfcio_put(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_put(spectx, ls, ea, size, tag & 0xf, tid << 8 | rid, MFC_CMD_PUT);
}

int _base_spe_mfcio_putb(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_put(spectx, ls, ea, size, tag & 0xf,
		tid << 8 | rid, MFC_CMD_PUTB);
	
}

int _base_spe_mfcio_putf(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_put(spectx, ls, ea, size, tag & 0xf,
			tid << 8 | rid, MFC_CMD_PUTF);
	
}


int _base_spe_mfcio_get(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_get(spectx, ls, ea, size, tag & 0xf,
				tid << 8 | rid, MFC_CMD_GET);
}

int _base_spe_mfcio_getb(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_get(spectx, ls, ea, size, tag & 0xf,
				tid << 8 | rid, MFC_CMD_GETB);
}

int _base_spe_mfcio_getf(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_get(spectx, ls, ea, size, tag & 0xf,
				tid << 8 | rid, MFC_CMD_GETF);
}

static int spe_mfcio_tag_status_read_all(spe_context_ptr_t spectx, 
                        unsigned int mask, unsigned int *tag_status)
{
	int fd;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		// fixme
		errno = ENOTSUP;
	} else {
		fd = open_if_closed(spectx, FD_MFC, 0);

		if (fsync(fd) != 0) {
			return -1;
		}

		return spe_read_tag_status_block(spectx, tag_status);
	}
	return -1;
}

static int spe_mfcio_tag_status_read_any(spe_context_ptr_t spectx,
					unsigned int mask, unsigned int *tag_status)
{
	return spe_read_tag_status_block(spectx, tag_status);
}

static int spe_mfcio_tag_status_read_immediate(spe_context_ptr_t spectx,
						     unsigned int mask, unsigned int *tag_status)
{
	return spe_read_tag_status_noblock(spectx, tag_status);
}



/* MFC Read tag status functions
 *
 */
static int spe_read_tag_status_block(spe_context_ptr_t spectx, unsigned int *tag_status)
{
	int fd;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		// fixme
		errno = ENOTSUP;
	} else {
		fd = open_if_closed(spectx, FD_MFC, 0);
		
		if (read(fd,tag_status,4) == 4) {
			return 0;
		}
	}
	return -1;
}

static int spe_read_tag_status_noblock(spe_context_ptr_t spectx, unsigned int *tag_status)
{
	struct pollfd poll_fd;
	
	int fd;
	unsigned int ret;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		// fixme
		errno = ENOTSUP;
	} else {
		fd = open_if_closed(spectx, FD_MFC, 0);
		
		poll_fd.fd = fd;
		poll_fd.events = POLLIN;

		ret = poll(&poll_fd, 1, 0);

		if (ret < 0)
			return -1;

		if (ret == 0 || !(poll_fd.revents | POLLIN)) {
			*tag_status = 0;
			return 0;
		}
	
		if (read(fd,tag_status,4) == 4) {
			return 0;
		}
	}
	return -1;
}

int _base_spe_mfcio_tag_status_read(spe_context_ptr_t spectx, unsigned int mask, unsigned int behavior, unsigned int *tag_status)
{
	if ( mask != 0 ) {
		errno = ENOTSUP;
		return -1;
	}

	switch (behavior) {
	case SPE_TAG_ALL:
		return spe_mfcio_tag_status_read_all(spectx, mask, tag_status);
	case SPE_TAG_ANY:
		return spe_mfcio_tag_status_read_any(spectx, mask, tag_status);
	case SPE_TAG_IMMEDIATE:
		return spe_mfcio_tag_status_read_immediate(spectx, mask, tag_status);
	default:
		errno = EINVAL;
		return -1;
	}
}
