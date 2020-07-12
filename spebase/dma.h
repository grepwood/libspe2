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

#ifndef _dma_h_
#define _dma_h_

#include <stdint.h>

#include "spebase.h"

struct mfc_command_parameter_area {
	uint32_t pad;	/* reserved		 */
	uint32_t lsa;	/* local storage address */
	uint64_t ea;	/* effective address	 */
	uint16_t size;	/* transfer size	 */
	uint16_t tag;	/* command tag		 */
	uint16_t class;	/* class ID		 */
	uint16_t cmd;	/* command opcode	 */
};

enum mfc_cmd {
	MFC_CMD_PUT  = 0x20,
	MFC_CMD_PUTB = 0x21,
	MFC_CMD_PUTF = 0x22,
	MFC_CMD_GET  = 0x40,
	MFC_CMD_GETB = 0x41,
	MFC_CMD_GETF = 0x42,
};

#endif
