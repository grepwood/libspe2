/* libspe - A wrapper library to adapt the JSRE SPE usage model to SPUFS
 * Copyright (C) 2005 IBM Corp.
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software Foundation,
 *   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <libspe.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/poll.h>

#include "spe.h"
#include "create.h"

#define __PRINTF(fmt, args...) { fprintf(stderr,fmt , ## args); }
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
#else
#define DEBUG_PRINTF(fmt, args...)
#endif


int spe_mfc_put (speid_t spe, unsigned src, void *dst,
		unsigned size, unsigned short tag,
		unsigned char tid, unsigned char rid)
{
	struct thread_store *speid = spe;
	
	return _base_spe_mfcio_put(speid->spectx, 
                        src, 
                        dst, 
                        size, 
                        tag, 
                        tid, 
                        rid);
}

int spe_mfc_putf(speid_t spe, unsigned src, void *dst,
		unsigned size, unsigned short tag,
		unsigned char tid, unsigned char rid)
{
	struct thread_store *speid = spe;

	return _base_spe_mfcio_putf(speid->spectx, 
                        src, 
                        dst, 
                        size, 
                        tag, 
                        tid, 
                        rid);
}

int spe_mfc_putb(speid_t spe, unsigned src, void *dst,
		unsigned size, unsigned short tag,
		unsigned char tid, unsigned char rid)
{
	struct thread_store *speid = spe;
	
	return _base_spe_mfcio_putb(speid->spectx, 
                        src, 
                        dst, 
                        size, 
                        tag, 
                        tid, 
                        rid);
}

int spe_mfc_get (speid_t spe, unsigned dst, void *src,
		unsigned size, unsigned short tag,
		unsigned char tid, unsigned char rid)
{
	struct thread_store *speid = spe;
	
	return _base_spe_mfcio_get(speid->spectx, 
                        dst, 
                        src, 
                        size, 
                        tag, 
                        tid, 
                        rid);
}

int spe_mfc_getf(speid_t spe, unsigned dst, void *src,
		unsigned size, unsigned short tag,
		unsigned char tid, unsigned char rid)
{
	struct thread_store *speid = spe;

	return _base_spe_mfcio_getf(speid->spectx, 
                        dst, 
                        src, 
                        size, 
                        tag, 
                        tid, 
                        rid);
}

int spe_mfc_getb(speid_t spe, unsigned dst, void *src,
		unsigned size, unsigned short tag,
		unsigned char tid, unsigned char rid)
{
	struct thread_store *speid = spe;

	return _base_spe_mfcio_getb(speid->spectx, 
                        dst, 
                        src, 
                        size, 
                        tag, 
                        tid, 
                        rid);
}
/* MFC Read tag status functions
 *
 */
static int read_tag_status_noblock(speid_t speid)
{
	struct thread_store *spe = speid;

	int r_read = 0;
	unsigned int ret;
	
	int mfcfd;
	mfcfd = _base_spe_open_if_closed(spe->spectx, FD_MFC, 0);

	r_read = read(mfcfd,&ret,4);

	if (r_read == 4)
	{
		return ret;
	}

	return -1;
}

static int read_tag_status_async(speid_t speid)
{
	struct thread_store *spe = speid;
	struct pollfd poll_fd;
	
	int r_read = 0;
	unsigned int ret;
	
	int mfcfd;
	mfcfd = _base_spe_open_if_closed(spe->spectx, FD_MFC, 0);

	poll_fd.fd = mfcfd;
	poll_fd.events = POLLIN;

	ret = poll(&poll_fd, 1, -1);

	if (ret < 0 || !(poll_fd.revents | POLLIN))
		return -1;
	
	r_read = read(mfcfd,&ret,4);

	if (r_read == 4)
	{
		return ret;
	}

	return -1;
}

int spe_mfc_read_tag_status_all(speid_t speid, unsigned int mask)
{
	int status;
	
	if ((status = read_tag_status_noblock(speid)) == -1)
		return -1;

	while ((status & mask) != mask)
	{
		if ((status = read_tag_status_async(speid)) == -1)
			return -1;
	}

	return status & mask;
}

int spe_mfc_read_tag_status_any(speid_t speid, unsigned int mask)
{
	int status;
	
	if ((status = read_tag_status_noblock(speid)) == -1)
		return -1;

	while ((status & mask) == 0)
	{
		if ((status = read_tag_status_async(speid)) == -1)
			return -1;
	}

	return status & mask;
}

int spe_mfc_read_tag_status_immediate(speid_t speid, unsigned int mask)
{
	int status;
	
	if ((status = read_tag_status_noblock(speid)) == -1)
		return -1;
	
	return  status & mask ;
}

