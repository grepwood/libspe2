/*
 * libspe2 - A wrapper library to adapt the JSRE SPE usage model to SPUFS
 * Copyright (C) 2005 IBM Corp.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software Foundation,
 *   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "spebase.h"
#include <elf.h>

#define LS_SIZE                       0x40000	/* 256K (in bytes) */

#define SPE_LDR_PROG_start    (LS_SIZE - 512)	// location of spe_ld.so prog
#define SPE_LDR_PARAMS_start  (LS_SIZE - 128)	// location of spe_ldr_params

typedef union
{
	unsigned long long ull;
	unsigned int ui[2];
} addr64;

struct spe_ld_info
{
	unsigned int entry;	
};

/*
 * Global API : */

int check_spe_elf(Elf32_Ehdr *ehdr);

int verify_spe_elf_image(spe_program_handle_t *handle);

int load_spe_elf (spe_program_handle_t *handle, void *ld_buffer,
		  struct spe_ld_info *ld_info);
		  
int spe_parse_isolated_elf(spe_program_handle_t *handle,
		uint64_t *addr, uint32_t *size);
		
int toe_ear (spe_program_handle_t *speh);
		  
