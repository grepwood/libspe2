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
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

#include "create.h"
#include "mbox.h"

/**
 * SPE Mailbox Communication
 * -------------------------
 */

static __inline__ int _base_spe_out_mbox_read_ps(spe_context_ptr_t spectx,
                        unsigned int mbox_data[], 
                        int count)
{
	volatile struct spe_spu_control_area *cntl_area =
        	spectx->base_private->cntl_mmap_base;
	int total;
	int entries;

	_base_spe_context_lock(spectx, FD_MBOX);
	total = 0;
	while (total < count) {
		entries = cntl_area->SPU_Mbox_Stat & 0xFF;
		if (entries == 0) break;  // no entries
		if (entries > count - total) entries = count - total; // don't read more than we need;
		while(entries--) {
			mbox_data[total] = cntl_area->SPU_Out_Mbox;
			total ++;
		}
	}
	_base_spe_context_unlock(spectx, FD_MBOX);
	return total;
}

int _base_spe_out_mbox_read(spe_context_ptr_t spectx, 
                        unsigned int mbox_data[], 
                        int count)
{
	int rc;

	if (mbox_data == NULL || count < 1){
		errno = EINVAL;
		return -1;
	}

	if (spectx->base_private->flags & SPE_MAP_PS) {
		rc = _base_spe_out_mbox_read_ps(spectx, mbox_data, count);
	} else {
		rc = read(_base_spe_open_if_closed(spectx,FD_MBOX, 0), mbox_data, count*4);
		DEBUG_PRINTF("%s read rc: %d\n", __FUNCTION__, rc);
		if (rc != -1) {
			rc /= 4;
		} else {
			if (errno == EAGAIN ) { // no data ready to be read
				errno = 0;
				rc = 0;
			}
		}
	}
	return rc;
}

static __inline__ int _base_spe_in_mbox_write_ps(spe_context_ptr_t spectx,
                        unsigned int *mbox_data, 
                        int count)
{
	volatile struct spe_spu_control_area *cntl_area =
        	spectx->base_private->cntl_mmap_base;
	int total = 0;
	unsigned int *aux;
	int space;

	_base_spe_context_lock(spectx, FD_WBOX);
	aux = mbox_data;
	while (total < count) {
		space = (cntl_area->SPU_Mbox_Stat >> 8) & 0xFF;
		if (space == 0) break;  // no space
		if (space > count - total) space = count - total; // don't write more than we have
		while  (space --) {
			cntl_area->SPU_In_Mbox = *aux++;
			total++;
		}
	}
	_base_spe_context_unlock(spectx, FD_WBOX);

	return total;
}

int _base_spe_in_mbox_write(spe_context_ptr_t spectx, 
                        unsigned int *mbox_data, 
                        int count, 
                        int behavior_flag)
{
	int rc;
	int total;
	unsigned int *aux;
	struct pollfd fds;

	if (mbox_data == NULL || count < 1){
		errno = EINVAL;
		return -1;
	}

	switch (behavior_flag) {
	case SPE_MBOX_ALL_BLOCKING: // write all, even if blocking
		total = rc = 0;
		if (spectx->base_private->flags & SPE_MAP_PS) {
			do {
				aux = mbox_data + total;
				total += _base_spe_in_mbox_write_ps(spectx, aux, count - total);
				if (total < count) { // we could not write everything, wait for space
					fds.fd = _base_spe_open_if_closed(spectx, FD_WBOX, 0);
					fds.events = POLLOUT;
					rc = poll(&fds, 1, -1);
					if (rc == -1 ) 
						return -1;
				}
			} while (total < count);
		} else {
			while (total < 4*count) {
				rc = write(_base_spe_open_if_closed(spectx,FD_WBOX, 0),
					   (const char *)mbox_data + total, 4*count - total);
				if (rc == -1) {
					break;
				}
				total += rc;
			}
			total /=4;
		}
		break;

	case  SPE_MBOX_ANY_BLOCKING: // write at least one, even if blocking
		total = rc = 0;
		if (spectx->base_private->flags & SPE_MAP_PS) {
			do {
				total = _base_spe_in_mbox_write_ps(spectx, mbox_data, count);
				if (total == 0) { // we could not anything, wait for space
					fds.fd = _base_spe_open_if_closed(spectx, FD_WBOX, 0);
					fds.events = POLLOUT;
					rc = poll(&fds, 1, -1);
					if (rc == -1 ) 
						return -1;
				}
			} while (total == 0);
		} else {
			rc = write(_base_spe_open_if_closed(spectx,FD_WBOX, 0), mbox_data, 4*count);
			total = rc/4;
		}
		break;

	case  SPE_MBOX_ANY_NONBLOCKING: // only write, if non blocking
		total = rc = 0;
		// write directly if we map the PS else write via spufs
		if (spectx->base_private->flags & SPE_MAP_PS) {
			total = _base_spe_in_mbox_write_ps(spectx, mbox_data, count);
		} else { 
			rc = write(_base_spe_open_if_closed(spectx,FD_WBOX_NB, 0), mbox_data, 4*count);
			if (rc == -1 && errno == EAGAIN) {
				rc = 0;
				errno = 0;
			}
			total = rc/4;
		}
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	if (rc == -1) {
		errno = EIO;
		return -1;
	}

	return total;
}

int _base_spe_in_mbox_status(spe_context_ptr_t spectx)
{
	int rc, ret;
	volatile struct spe_spu_control_area *cntl_area =
        	spectx->base_private->cntl_mmap_base;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		ret = (cntl_area->SPU_Mbox_Stat >> 8) & 0xFF;
	} else {
		rc = read(_base_spe_open_if_closed(spectx,FD_WBOX_STAT, 0), &ret, 4);
		if (rc != 4)
			ret = -1;
	}

	return ret;
	
}

int _base_spe_out_mbox_status(spe_context_ptr_t spectx)
{
        int rc, ret;
	volatile struct spe_spu_control_area *cntl_area =
        	spectx->base_private->cntl_mmap_base;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		ret = cntl_area->SPU_Mbox_Stat & 0xFF;
	} else {
        	rc = read(_base_spe_open_if_closed(spectx,FD_MBOX_STAT, 0), &ret, 4);
	        if (rc != 4)
        	        ret = -1;
	}

        return ret;
	
}

int _base_spe_out_intr_mbox_status(spe_context_ptr_t spectx)
{
        int rc, ret;
	volatile struct spe_spu_control_area *cntl_area =
        	spectx->base_private->cntl_mmap_base;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		ret = (cntl_area->SPU_Mbox_Stat >> 16) & 0xFF;
	} else {
	        rc = read(_base_spe_open_if_closed(spectx,FD_IBOX_STAT, 0), &ret, 4);
	        if (rc != 4)
        	        ret = -1;

	}
        return ret;
}

int _base_spe_out_intr_mbox_read(spe_context_ptr_t spectx, 
                        unsigned int mbox_data[], 
                        int count, 
                        int behavior_flag)
{
	int rc;
	int total;

	if (mbox_data == NULL || count < 1){
		errno = EINVAL;
		return -1;
	}

	switch (behavior_flag) {
	case SPE_MBOX_ALL_BLOCKING: // read all, even if blocking
		total = rc = 0;
		while (total < 4*count) {
			rc = read(_base_spe_open_if_closed(spectx,FD_IBOX, 0),
				  (char *)mbox_data + total, 4*count - total);
			if (rc == -1) {
				break;
			}
			total += rc;
		}
		break;

	case  SPE_MBOX_ANY_BLOCKING: // read at least one, even if blocking
		total = rc = read(_base_spe_open_if_closed(spectx,FD_IBOX, 0), mbox_data, 4*count);
		break;

	case  SPE_MBOX_ANY_NONBLOCKING: // only reaad, if non blocking
		rc = read(_base_spe_open_if_closed(spectx,FD_IBOX_NB, 0), mbox_data, 4*count);
		if (rc == -1 && errno == EAGAIN) {
			rc = 0;
			errno = 0;
		}
		total = rc;
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	if (rc == -1) {
		errno = EIO;
		return -1;
	}

	return total / 4;
}

int _base_spe_signal_write(spe_context_ptr_t spectx, 
                        unsigned int signal_reg, 
                        unsigned int data )
{
	int rc;

	if (spectx->base_private->flags & SPE_MAP_PS) {
		if (signal_reg == SPE_SIG_NOTIFY_REG_1) {
			spe_sig_notify_1_area_t *sig = spectx->base_private->signal1_mmap_base;

			sig->SPU_Sig_Notify_1 = data;
		} else if (signal_reg == SPE_SIG_NOTIFY_REG_2) {
			spe_sig_notify_2_area_t *sig = spectx->base_private->signal2_mmap_base;

			sig->SPU_Sig_Notify_2 = data;
		} else {
			errno = EINVAL;
			return -1;
		}
		rc = 0;
	} else {
		if (signal_reg == SPE_SIG_NOTIFY_REG_1)
			rc = write(_base_spe_open_if_closed(spectx,FD_SIG1, 0), &data, 4);
		else if (signal_reg == SPE_SIG_NOTIFY_REG_2)
			rc = write(_base_spe_open_if_closed(spectx,FD_SIG2, 0), &data, 4);
		else {
			errno = EINVAL;
			return -1;
		}
		
		if (rc == 4)
			rc = 0;
	
		if (signal_reg == SPE_SIG_NOTIFY_REG_1)
			_base_spe_close_if_open(spectx,FD_SIG1);
		else if (signal_reg == SPE_SIG_NOTIFY_REG_2)
			_base_spe_close_if_open(spectx,FD_SIG2);
	}

	return rc;
}



