/*
 * libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
 * Copyright (C) 2008 IBM Corp.
 *
 * Author: Jeremy Kerr <jk@ozlabs.org>
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

#ifndef _regs_h_
#define _regs_h_

#include "spebase.h"

struct spe_reg_state {
	struct spe_reg128 r3, r4, r5, r6;
	struct spe_reg128 entry;
};

int _base_spe_setup_registers(struct spe_context *spe,
		struct spe_reg_state *regs,
		unsigned int *entry);

#endif
