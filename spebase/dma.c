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
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/poll.h>

#include "create.h"
#include "dma.h"

static int spe_read_tag_status_block(spe_context_ptr_t spectx, unsigned int mask, unsigned int *tag_status);
static int spe_read_tag_status_noblock(spe_context_ptr_t spectx, unsigned int mask, unsigned int *tag_status);

static int issue_mfc_command(spe_context_ptr_t spectx, unsigned lsa, void *ea,
			     unsigned size, unsigned tag, unsigned tid, unsigned rid,
			     enum mfc_cmd cmd)
{
	int ret;

	DEBUG_PRINTF("queuing DMA %x %lx %x %x %x %x %x\n", lsa,
		(unsigned long)ea, size, tag, tid, rid, (unsigned)cmd);

	/* tag 16-31 are reserved by kernel */
	if (tag > 0x0f || tid > 0xff || rid > 0xff) {
		errno = EINVAL;
		return -1;
	}

	if (spectx->base_private->flags & SPE_MAP_PS) {
		volatile struct spe_mfc_command_area *cmd_area =
			spectx->base_private->mfc_mmap_base;
		unsigned int eal = (uintptr_t) ea & 0xFFFFFFFF;
		unsigned int eah = (unsigned long long)(uintptr_t) ea >> 32;
		_base_spe_context_lock(spectx, FD_MFC);
		spectx->base_private->active_tagmask |= 1 << tag;
		DEBUG_PRINTF("set active tagmask = 0x%04x, tag=%i\n",spectx->base_private->active_tagmask,tag);
		while ((cmd_area->MFC_QStatus & 0x0000FFFF) == 0) ;
		do {
			cmd_area->MFC_LSA         = lsa;
			cmd_area->MFC_EAH         = eah;
			cmd_area->MFC_EAL         = eal;
			cmd_area->MFC_Size_Tag    = (size << 16) | tag;
			cmd_area->MFC_ClassID_CMD = (tid << 24) | (rid << 16) | cmd;

			ret = cmd_area->MFC_CMDStatus & 0x00000003;
		} while (ret); // at least one of the two bits is set
		_base_spe_context_unlock(spectx, FD_MFC);
		return 0;
	}
	else {
		int fd;
		fd = _base_spe_open_if_closed(spectx, FD_MFC, 0);
		if (fd != -1) {
			struct mfc_command_parameter_area parm = {
				.lsa   = lsa,
				.ea    = (unsigned long) ea,
				.size  = size,
				.tag   = tag,
				.class = (tid << 8) | rid,
				.cmd   = cmd,
			};
			ret = write(fd, &parm, sizeof (parm));
			if ((ret < 0) && (errno != EIO)) {
				perror("spe_do_mfc_put: internal error");
			}
			return ret < 0 ? -1 : 0;
		}
	}
	/* the kernel does not support DMA */
	return 1;
}

static int spe_do_mfc_put(spe_context_ptr_t spectx, unsigned src, void *dst,
			  unsigned size, unsigned tag, unsigned tid, unsigned rid,
			  enum mfc_cmd cmd)
{
	int ret;
	ret = issue_mfc_command(spectx, src, dst, size, tag, tid, rid, cmd);
	if (ret <= 0) {
		return ret;
	}
	else {
		/* the kernel does not support DMA, so just copy directly */
		if (spectx->base_private->mem_mmap_base == MAP_FAILED) {
			errno = EINVAL;
			return -1;
		}
		memcpy(dst, spectx->base_private->mem_mmap_base + src, size);
		return 0;
	}
}

static int spe_do_mfc_get(spe_context_ptr_t spectx, unsigned int dst, void *src,
			  unsigned int size, unsigned int tag, unsigned tid, unsigned rid,
			  enum mfc_cmd cmd)
{
	int ret;
	ret = issue_mfc_command(spectx, dst, src, size, tag, tid, rid, cmd);
	if (ret <= 0) {
		return ret;
	}
	else {
		/* the kernel does not support DMA, so just copy directly */
		if (spectx->base_private->mem_mmap_base == MAP_FAILED) {
			errno = EINVAL;
			return -1;
		}
		memcpy(spectx->base_private->mem_mmap_base + dst, src, size);
		return 0;
	}
}

int _base_spe_mfcio_put(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_put(spectx, ls, ea, size, tag, tid, rid, MFC_CMD_PUT);
}

int _base_spe_mfcio_putb(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_put(spectx, ls, ea, size, tag, tid, rid, MFC_CMD_PUTB);
}

int _base_spe_mfcio_putf(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_put(spectx, ls, ea, size, tag, tid, rid, MFC_CMD_PUTF);
}


int _base_spe_mfcio_get(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_get(spectx, ls, ea, size, tag, tid, rid, MFC_CMD_GET);
}

int _base_spe_mfcio_getb(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_get(spectx, ls, ea, size, tag, rid, rid, MFC_CMD_GETB);
}

int _base_spe_mfcio_getf(spe_context_ptr_t spectx, 
                        unsigned int ls, 
                        void *ea, 
                        unsigned int size, 
                        unsigned int tag, 
                        unsigned int tid, 
                        unsigned int rid)
{
	return spe_do_mfc_get(spectx, ls, ea, size, tag, tid, rid, MFC_CMD_GETF);
}

static int spe_mfcio_tag_status_read_all(spe_context_ptr_t spectx, 
                        unsigned int mask, unsigned int *tag_status)
{
	int fd;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		return spe_read_tag_status_block(spectx, mask, tag_status);
	} else {
		fd = _base_spe_open_if_closed(spectx, FD_MFC, 0);
		if (fd == -1) {
			return -1;
		}

		if (fsync(fd) != 0) {
			return -1;
		}

		return spe_read_tag_status_block(spectx, mask, tag_status);
	}
}

static int spe_mfcio_tag_status_read_any(spe_context_ptr_t spectx,
					unsigned int mask, unsigned int *tag_status)
{
	return spe_read_tag_status_block(spectx, mask, tag_status);
}

static int spe_mfcio_tag_status_read_immediate(spe_context_ptr_t spectx,
						     unsigned int mask, unsigned int *tag_status)
{
	return spe_read_tag_status_noblock(spectx, mask, tag_status);
}



/* MFC Read tag status functions
 *
 */
static int spe_read_tag_status_block(spe_context_ptr_t spectx, unsigned int mask, unsigned int *tag_status)
{
	if (spectx->base_private->flags & SPE_MAP_PS) {
		volatile struct spe_mfc_command_area *cmd_area =
			spectx->base_private->mfc_mmap_base;
		_base_spe_context_lock(spectx, FD_MFC);
		cmd_area->Prxy_QueryMask = mask;
		__asm__ ("eieio");
		do {
			*tag_status = cmd_area->Prxy_TagStatus;
			spectx->base_private->active_tagmask ^= *tag_status;
			DEBUG_PRINTF("unset active tagmask = 0x%04x, tag_status = 0x%04x\n",
							spectx->base_private->active_tagmask,*tag_status);
		} while (*tag_status ^ mask);
		_base_spe_context_unlock(spectx, FD_MFC);
		return 0;
	} else {
		int fd;
		fd = _base_spe_open_if_closed(spectx, FD_MFC, 0);
		if (fd == -1) {
			return -1;
		}
		
		if (read(fd,tag_status,4) == 4) {
			return 0;
		}
	}
	return -1;
}

static int spe_read_tag_status_noblock(spe_context_ptr_t spectx, unsigned int mask, unsigned int *tag_status)
{
	unsigned int ret;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		volatile struct spe_mfc_command_area *cmd_area =
			spectx->base_private->mfc_mmap_base;

		_base_spe_context_lock(spectx, FD_MFC);
		cmd_area->Prxy_QueryMask = mask;
		__asm__ ("eieio");
		*tag_status =  cmd_area->Prxy_TagStatus;
		spectx->base_private->active_tagmask ^= *tag_status;
		DEBUG_PRINTF("unset active tagmask = 0x%04x, tag_status = 0x%04x\n",
						spectx->base_private->active_tagmask,*tag_status);
		_base_spe_context_unlock(spectx, FD_MFC);
		return 0;
	} else {
		struct pollfd poll_fd;
		int fd;

		fd = _base_spe_open_if_closed(spectx, FD_MFC, 0);
		if (fd == -1) {
			return -1;
		}
		
		poll_fd.fd = fd;
		poll_fd.events = POLLIN;

		ret = poll(&poll_fd, 1, 0);

		if (ret < 0)
			return -1;

		if (ret == 0 || !(poll_fd.revents & POLLIN)) {
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
		if (!(spectx->base_private->flags & SPE_MAP_PS)) 
			mask = 0;
	} else {
		if ((spectx->base_private->flags & SPE_MAP_PS))
			mask = spectx->base_private->active_tagmask;
	}

	if (!tag_status) {
		errno = EINVAL;
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

int _base_spe_mssync_start(spe_context_ptr_t spectx)
{
	int ret, fd;
	unsigned int data = 1; /* Any value can be written here */

	volatile struct spe_mssync_area *mss_area = 
                spectx->base_private->mssync_mmap_base;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		mss_area->MFC_MSSync = data; 
		return 0;
	} else {
		fd = _base_spe_open_if_closed(spectx, FD_MSS, 0);
		if (fd != -1) {
			ret = write(fd, &data, sizeof (data));
			if ((ret < 0) && (errno != EIO)) {
				perror("spe_mssync_start: internal error");
			}
			return ret < 0 ? -1 : 0;
		} else 
			return -1;
	}
}

int _base_spe_mssync_status(spe_context_ptr_t spectx)
{
	int ret, fd;
	unsigned int data;

	volatile struct spe_mssync_area *mss_area = 
                spectx->base_private->mssync_mmap_base;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		return  mss_area->MFC_MSSync;
	} else {
		fd = _base_spe_open_if_closed(spectx, FD_MSS, 0);
		if (fd != -1) {
			ret = read(fd, &data, sizeof (data));
			if ((ret < 0) && (errno != EIO)) {
				perror("spe_mssync_start: internal error");
			}
			return ret < 0 ? -1 : data;
		} else 
			return -1;
	}
}


