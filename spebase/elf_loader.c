/*
 * libspe2 - A wrapper to allow direct execution of SPE binaries
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

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "elf_loader.h"
#include "spebase.h"

#ifdef DEBUG
static void display_debug_output(Elf32_Ehdr *elf_start, Elf32_Shdr *sh);
#endif /*DEBUG*/

#define __PRINTF(fmt, args...) { fprintf(stderr,fmt , ## args); }
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
#define TAG DEBUG_PRINTF("TAG: %d@%s\n", __LINE__, __FILE__);
#else
#define DEBUG_PRINTF(fmt, args...)
#define TAG
#endif

static const unsigned char expected[EI_PAD] = {
	[EI_MAG0] = ELFMAG0,
	[EI_MAG1] = ELFMAG1,
	[EI_MAG2] = ELFMAG2,
	[EI_MAG3] = ELFMAG3,
	[EI_CLASS] = ELFCLASS32,
	[EI_DATA] = ELFDATA2MSB,
	[EI_VERSION] = EV_CURRENT,
	[EI_OSABI] = ELFOSABI_SYSV,
	[EI_ABIVERSION] = 0
};

static int
check_spe_elf(Elf32_Ehdr *ehdr)
{
	/* Validate ELF */
	if (memcmp (ehdr->e_ident, expected, EI_PAD) != 0)
	  {
		  DEBUG_PRINTF ("invalid ELF header.\n");
		  DEBUG_PRINTF ("expected 0x%016llX != 0x%016llX\n",
				*(long long *) expected, *(long long *) (ehdr->e_ident));
		  errno = EINVAL;
		  return -errno;
	  }

	/* Validate the machine type */
	if (ehdr->e_machine != 0x17)
	  {
		  DEBUG_PRINTF ("not an SPE ELF object");
		  errno = EINVAL;
		  return -errno;
	  }

	/* Validate ELF object type. */
	if (ehdr->e_type != ET_EXEC)
	  {
		  DEBUG_PRINTF ("invalid SPE ELF type.\n");
		  DEBUG_PRINTF ("SPE type %d != %d\n", ehdr->e_type, ET_EXEC);
		  errno = EINVAL;
		  DEBUG_PRINTF ("parse_spu_elf(): errno=%d.\n", errno);
		  return -errno;
	  }

	return 0;

}
/**
 * verifies integrity of an SPE image
 */
int  
_base_spe_verify_spe_elf_image(spe_program_handle_t *handle)
{
	Elf32_Ehdr *ehdr;
	void *elf_start;
	
	elf_start = handle->elf_image;
	ehdr = (Elf32_Ehdr *)(handle->elf_image);

	return check_spe_elf(ehdr);
}

int
_base_spe_parse_isolated_elf(spe_program_handle_t *handle,
		uint64_t *addr, uint32_t *size)
{
	Elf32_Ehdr *ehdr = (Elf32_Ehdr *)handle->elf_image;
	Elf32_Phdr *phdr;

	if (!ehdr) {
		DEBUG_PRINTF("No ELF image has been loaded\n");
		errno = EINVAL;
		return -errno;
	}

	if (ehdr->e_phentsize != sizeof(*phdr)) {
		DEBUG_PRINTF("Invalid program header format (phdr size=%d)\n",
				ehdr->e_phentsize);
		errno = EINVAL;
		return -errno;
	}

	if (ehdr->e_phnum != 1) {
		DEBUG_PRINTF("Invalid program header count (%d), expected 1\n",
				ehdr->e_phnum);
		errno = EINVAL;
		return -errno;
	}

	phdr = (Elf32_Phdr *)(handle->elf_image + ehdr->e_phoff);

	if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0) {
		DEBUG_PRINTF("SPE program segment is not loadable (type=%x)\n",
				phdr->p_type);
		errno = EINVAL;
		return -errno;
	}

	if (addr)
		*addr = (uint64_t)(unsigned long)
			(handle->elf_image + phdr->p_offset);

	if (size)
		*size = phdr->p_memsz;

	return 0;
}

static int
overlay(Elf32_Phdr *ph, Elf32_Phdr *prev_ph)
{
	/*
	 * If our ph segment virtual address fits within that of the
	 * previous ph, this is an overlay.
	 */
	if (prev_ph && (ph->p_vaddr >= prev_ph->p_vaddr) &&
	    (ph->p_vaddr < (prev_ph->p_vaddr + prev_ph->p_memsz)))
		return 1;
	else
		return 0;
}

static void
copy_to_ld_buffer(spe_program_handle_t *handle, void *buffer, Elf32_Phdr
		  *ph, Elf32_Off toe_addr, long toe_size)
{
	void *start = handle->elf_image;

	DEBUG_PRINTF("SPE_LOAD %p (0x%x) -> %p (0x%x) (%i bytes)\n",
		     buffer + ph->p_vaddr, ph->p_vaddr, start + ph->p_offset,
		     ph->p_offset, ph->p_filesz);

	/* toe segment comes from the shadow */
	if (ph->p_vaddr == toe_addr) {
		/* still makes a copy if toe is buried with other
		 * sections */
		if (toe_size != ph->p_filesz && ph->p_filesz) {
			DEBUG_PRINTF("loading base copy\n");
			memcpy(buffer + ph->p_vaddr, start + ph->p_offset,
			       ph->p_filesz);
		}

		/* overlay only the total toe section size */
		DEBUG_PRINTF("loading toe %X %X\n", ph->p_offset, toe_addr);
		memcpy(buffer + ph->p_vaddr, handle->toe_shadow, toe_size);
	} else if (ph->p_filesz) {
		memcpy(buffer + ph->p_vaddr, start + ph->p_offset,
		       ph->p_filesz);
	}
	DEBUG_PRINTF("done ...\n");
}

/* Apply certain R_SPU_PPU* relocs in RH to SH.  We only handle relocs
   without a symbol, which are to locations within ._ea.  */

static void
apply_relocations(spe_program_handle_t *handle, Elf32_Shdr *rh, Elf32_Shdr *sh)
{
#define R_SPU_PPU32 15
#define R_SPU_PPU64 16
	void *start = handle->elf_image;
	Elf32_Rela *r, *r_end;
	/* Relocations in an executable specify r_offset as a virtual
	   address, but we are applying the reloc in the image before
	   the section has been copied to its destination sh_addr.
	   Adjust so as to poke relative to the image base.  */
	void *reloc_base = start + sh->sh_offset - sh->sh_addr;

	r = start + rh->sh_offset;
	r_end = (void *)r + rh->sh_size;
	DEBUG_PRINTF("apply_relocations: %p, %#x\n", r, rh->sh_size);
	for (; r < r_end; ++r)
	{
		if (r->r_info == ELF32_R_INFO(0,R_SPU_PPU32)) {
			/* v is in ._ea */
			Elf32_Word *loc = reloc_base + r->r_offset;
			Elf32_Word v = (unsigned long)start + r->r_addend;
			/* Don't dirty pages unnecessarily.  */
			if (*loc != v)
				*loc = v;
			DEBUG_PRINTF("PPU32(%p) = %#x\n", loc, v);
		} else if (r->r_info == ELF32_R_INFO(0,R_SPU_PPU64)) {
			/* v is in ._ea */
			Elf64_Xword *loc = reloc_base + r->r_offset;
			Elf64_Xword v = (unsigned long)start + r->r_addend;
			if (*loc != v)
				*loc = v;
			DEBUG_PRINTF("PPU64(%p) = %#llx\n", loc, v);
		}
	}
}

int
_base_spe_load_spe_elf (spe_program_handle_t *handle, void *ld_buffer, struct spe_ld_info *ld_info)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	Elf32_Phdr *ph, *prev_ph;

	Elf32_Shdr *shdr;
	Elf32_Shdr *sh;

	Elf32_Off  toe_addr = 0;
	long	toe_size = 0;

	char* str_table = 0;

	int num_load_seg = 0;
	void *elf_start;
	int ret;
	
	DEBUG_PRINTF ("load_spe_elf(%p, %p)\n", handle, ld_buffer);

	elf_start = handle->elf_image;

	DEBUG_PRINTF ("load_spe_elf(%p, %p)\n", handle->elf_image, ld_buffer);
	ehdr = (Elf32_Ehdr *)(handle->elf_image);

	/* Check for a Valid SPE ELF Image (again) */
	if ((ret=check_spe_elf(ehdr)))
		return ret;

	/* Start processing headers */
	phdr = (Elf32_Phdr *) ((char *) ehdr + ehdr->e_phoff);
	shdr = (Elf32_Shdr *) ((char *) ehdr + ehdr->e_shoff);
	str_table = (char*)ehdr + shdr[ehdr->e_shstrndx].sh_offset;

	/* traverse the sections to locate the toe segment */
	/* by specification, the toe sections are grouped together in a segment */
	for (sh = shdr; sh < &shdr[ehdr->e_shnum]; ++sh)
	{
		DEBUG_PRINTF("section name: %s ( start: 0x%04x, size: 0x%04x)\n", str_table+sh->sh_name, sh->sh_offset, sh->sh_size );
		if (sh->sh_type == SHT_RELA)
			apply_relocations(handle, sh, &shdr[sh->sh_info]);
		if (strcmp(".toe", str_table+sh->sh_name) == 0) {
			DEBUG_PRINTF("section offset: %d\n", sh->sh_offset);
			toe_size += sh->sh_size;
			if ((toe_addr == 0) || (toe_addr > sh->sh_addr))
				toe_addr = sh->sh_addr;
		}
		/* Disabled : Actually not needed, only good for testing
		if (strcmp(".bss", str_table+sh->sh_name) == 0) {
			DEBUG_PRINTF("zeroing .bss section:\n");
			DEBUG_PRINTF("section offset: 0x%04x\n", sh->sh_offset);
			DEBUG_PRINTF("section size: 0x%04x\n", sh->sh_size);
			memset(ld_buffer + sh->sh_offset, 0, sh->sh_size);
		}  */
		
#ifdef DEBUG
		if (strcmp(".note.spu_name", str_table+sh->sh_name) == 0) 
			display_debug_output(elf_start, sh);
#endif /*DEBUG*/
	}

	/*
	 * Load all PT_LOAD segments onto the SPE local store buffer.
	 */
	DEBUG_PRINTF("Segments: 0x%x\n", ehdr->e_phnum);
	for (ph = phdr, prev_ph = NULL; ph < &phdr[ehdr->e_phnum]; ++ph) {
		switch (ph->p_type) {
		case PT_LOAD:
			if (!overlay(ph, prev_ph)) {
				if (ph->p_filesz < ph->p_memsz) {
					DEBUG_PRINTF("padding loaded image with zeros:\n");
					DEBUG_PRINTF("start: 0x%04x\n", ph->p_vaddr + ph->p_filesz);
					DEBUG_PRINTF("length: 0x%04x\n", ph->p_memsz - ph->p_filesz);
					memset(ld_buffer + ph->p_vaddr + ph->p_filesz, 0, ph->p_memsz - ph->p_filesz);
				}
				copy_to_ld_buffer(handle, ld_buffer, ph,
						  toe_addr, toe_size);
				num_load_seg++;
			}
			break;
		case PT_NOTE:
			DEBUG_PRINTF("SPE_LOAD found PT_NOTE\n");
			break;
		}
	}
	if (num_load_seg == 0)
	  {
		  DEBUG_PRINTF ("no segments to load");
		  errno = EINVAL;
		  return -errno;
	  }

	/* Remember where the code wants to be started */
	ld_info->entry = ehdr->e_entry;
	DEBUG_PRINTF ("entry = 0x%x\n", ehdr->e_entry);

	return 0;

}

#ifdef DEBUG
static void
display_debug_output(Elf32_Ehdr *elf_start, Elf32_Shdr *sh)
{
	typedef struct
	{
		unsigned long namesz;
		unsigned long descsz;
		unsigned long type;
		char name[8];
		char lookupname[32];
	} ELF_NOTE;

	ELF_NOTE *note = (ELF_NOTE *)((void *)elf_start+sh->sh_offset);
	printf ("Loading SPE program : %s\n", note->lookupname);
	printf ("SPU LS Entry Addr   : 0x%05x\n", elf_start->e_entry);
}
#endif /*DEBUG*/

static int
toe_check_syms(Elf32_Ehdr *ehdr, Elf32_Shdr *sh)
{
	Elf32_Sym  *sym, *sym_hdr, *sym_end;
	Elf32_Shdr *shdr;
	char *str_table;
	char *sym_name;
	int ret;

	shdr = (Elf32_Shdr*) ((char*) ehdr + ehdr->e_shoff);
	sym_hdr = (Elf32_Sym*) ((char*) ehdr + sh->sh_offset);
	sym_end = (Elf32_Sym*) ((char*) ehdr + sh->sh_offset + sh->sh_size);
	str_table = (char*)ehdr + shdr[sh->sh_link].sh_offset;

	ret = 0;
	for (sym = sym_hdr; sym < sym_end; sym++)
		if (sym->st_name) {
			sym_name = str_table + sym->st_name;
			if ((strncmp(sym_name, "_EAR_", 5) == 0) &&
			    (strcmp(sym_name, "_EAR_") != 0)) {
				/*
				 * We have a prefix of _EAR_ followed by
				 * something else. This is not currently
				 * (and might not ever be) supported: for
				 * a _EAR_foo, it requires a lookup of foo
				 * in the ppu ELF file.
				 */
				fprintf(stderr, "Invalid _EAR_ symbol '%s'\n",
					sym_name);
				errno = EINVAL;
				ret = 1;
			}
		}
	return ret;
}

int _base_spe_toe_ear (spe_program_handle_t *speh)
{
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr, *sh;
	char *str_table;
	char **ch;
	int ret;
	long toe_size;

	ehdr = (Elf32_Ehdr*) (speh->elf_image);
	shdr = (Elf32_Shdr*) ((char*) ehdr + ehdr->e_shoff);
	str_table = (char*)ehdr + shdr[ehdr->e_shstrndx].sh_offset;

	toe_size = 0;
	for (sh = shdr; sh < &shdr[ehdr->e_shnum]; ++sh)
		if (strcmp(".toe", str_table + sh->sh_name) == 0)
			toe_size += sh->sh_size;

	ret = 0;
	if (toe_size > 0) {
		for (sh = shdr; sh < &shdr[ehdr->e_shnum]; ++sh)
			if (sh->sh_type == SHT_SYMTAB || sh->sh_type ==
			    SHT_DYNSYM)
				ret = toe_check_syms(ehdr, sh);
		if (!ret && toe_size != 16) {
			/* Paranoia */
			fprintf(stderr, "Unexpected toe size of %ld\n",
				toe_size);
			errno = EINVAL;
			ret = 1;
		}
	}
	if (!ret && toe_size) {
		/*
		 * Allocate toe_shadow, and fill it with elf_image.
		 */
		speh->toe_shadow = malloc(toe_size);
		if (speh->toe_shadow) {
			ch = (char**) speh->toe_shadow;
			if (sizeof(char*) == 8) {
				ch[0] = (char*) speh->elf_image;
				ch[1] = 0;
			} else {
				ch[0] = 0;
				ch[1] = (char*) speh->elf_image;
				ch[2] = 0;
				ch[3] = 0;
			}
		} else {
			errno = ENOMEM;
			ret = 1;
		}
	}
	return ret;
}

