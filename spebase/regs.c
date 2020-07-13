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

#include <stdint.h>
#include <string.h>

#include "spebase.h"
#include "regs.h"

/**
 * A little PIC trampoline that is written to the end of local store, which
 * will later be overwritten by the stack.
 *
 * This trampoline provides an area for a struct spe_reg_state, and a little
 * code to load the appropriate areas of the reg_state into the actual regs,
 * then branch to the entry point of the program
 *
 * After loading this trampoline, we need to copy the spe_reg_state struct
 * into the base address of the trampoline.
 */

#if 0
reg_state:
	.space	80			/* sizeof(spe_reg_state) */
_start:
	lqr	r3,reg_state + 0	/* r3 = spe_reg_state.r3 */
	lqr	r4,reg_state + 16	/* r4 = spe_reg_state.r4 */
	lqr	r5,reg_state + 32	/* r5 = spe_reg_state.r5 */

	/* we have two alignment requirements here: reg_state needs to sit
	 * on a quadword boundary, and the bisl instruction needs to be
	 * the last word before the backchain pointer. So, align here, then
	 * add three instructions after the alignment, leaving bisl on the
	 * 4th word. */
	.balign	16

	lqr	r6,reg_state + 48	/* r6 = spe_reg_state.r6 */
	lqr	r1,reg_state + 64	/* r1 = spe_reg_state.entry */
	il	r2,0			/* stack size: 0 = default */

	bisl	r1,r1			/* branch to the program entry, and
					   set the stack pointer to the
					   following word */
backchain:
	/* initial stack backchain pointer - NULL*/
	.long	0x0
	.long	0x0
	.long	0x0
	.long	0x0
#endif
static uint32_t reg_setup_trampoline[] = {
/* reg_state: */
	[sizeof(struct spe_reg_state) / sizeof(uint32_t)] =
/* _start: */
	0x33fff603, /* lqr     r3,0 <reg_state>       */
	0x33fff784, /* lqr     r4,10 <reg_state+0x10> */
	0x33fff905, /* lqr     r5,20 <reg_state+0x20> */
	0x00200000, /* lnop                           */
	0x33fffa06, /* lqr     r6,30 <reg_state+0x30> */
	0x33fffb81, /* lqr     r1,40 <reg_state+0x40> */
	0x40800002, /* il      r2,0                   */
	0x35200081, /* bisl    r1,r1                  */
/* backchain: */
	0x00000000, /* stop                           */
	0x00000000, /* stop                           */
	0x00000000, /* stop                           */
	0x00000000, /* stop                           */
};

int _base_spe_setup_registers(struct spe_context *spe,
		struct spe_reg_state *regs,
		unsigned int *entry)
{
	unsigned int base_addr = LS_SIZE - sizeof(reg_setup_trampoline);

	memcpy(spe->base_private->mem_mmap_base + base_addr,
			reg_setup_trampoline, sizeof(reg_setup_trampoline));

	memcpy(spe->base_private->mem_mmap_base + base_addr,
			regs, sizeof(*regs));

	*entry = base_addr + sizeof(struct spe_reg_state);

	return 0;
}

