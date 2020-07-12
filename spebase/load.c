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
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include "elf_loader.h"
#include "create.h"
#include "spebase.h"

#ifndef SPE_EMULATED_LOADER_FILE
#define SPE_EMULATED_LOADER_FILE "/usr/lib/spe/emulated-loader.bin"
#endif

/**
 * Register the SPE program's start address with the oprofile and gdb, by
 * writing to the object-id file.
 */
void _base_spe_program_load_complete(spe_context_ptr_t spectx)
{
	int objfd, len;
	char buf[20];
	spe_program_handle_t *program;

	program = spectx->base_private->loaded_program;

	if (!program || !program->elf_image) {
		DEBUG_PRINTF("%s called, but no program loaded\n", __func__);
		return;
	}

	objfd = openat(spectx->base_private->fd_spe_dir, "object-id", O_RDWR);
	if (objfd < 0)
		return;

	len = sprintf(buf, "%p", program->elf_image);
	write(objfd, buf, len + 1);
	close(objfd);

	__spe_context_update_event();
}

/**
 * Send the isolated app loading parameters to the SPE.
 *
 * Helper function for internal libspe use.
 *
 * @param spe The SPE thread to send the mailbox message to
 * @param addr_h  The high 32-bits of the address of the isolated app
 * @param addr_l  The low 32-bits of the address of the isolated app
 * @param size  The size of the isolated app binary
 * @return zero on success, non-zero on failure
 */
static inline int __write_isolated_load_params(struct spe_context *spe,
		uint32_t addr_h, uint32_t addr_l, uint32_t size)
{
	int fd = _base_spe_open_if_closed(spe, FD_WBOX, 0);

	if (fd < 0) {
		DEBUG_PRINTF("%s: can't open wbox\n", __FUNCTION__);
		return -1;
	}

	if ((write(fd, &addr_h, sizeof(uint32_t)) != sizeof(uint32_t)) ||
	    (write(fd, &addr_l, sizeof(uint32_t)) != sizeof(uint32_t)) ||
	    (write(fd, &size,   sizeof(uint32_t)) != sizeof(uint32_t))) {
		DEBUG_PRINTF("%s: error writing to wbox\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

/**
 * Initiate transfer of an isolated SPE app by the loader kernel.
 *
 * Helper function for internal libspe use.
 *
 * @param thread The SPE thread to load the app to
 * @param handle The handle to the spe program
 * @return zero on success, non-zero on failure;
 */
static int spe_start_isolated_app(struct spe_context *spe,
		spe_program_handle_t *handle)
{
	uint64_t addr;
	uint32_t size, addr_h, addr_l;

	if (_base_spe_parse_isolated_elf(handle, &addr, &size)) {
		DEBUG_PRINTF("%s: invalid isolated image\n", __FUNCTION__);
		errno = ENOEXEC;
		return -errno;
	}

	if (addr & 0xf) {
		DEBUG_PRINTF("%s: isolated image is incorrectly aligned\n",
				__FUNCTION__);
		errno = EINVAL;
		return -errno;
	}

	addr_l = (uint32_t)(addr & 0xffffffff);
	addr_h = (uint32_t)(addr >> 32);

	DEBUG_PRINTF("%s: Sending isolated app params: 0x%08x 0x%08x 0x%08x\n",
			__FUNCTION__, addr_h, addr_l, size);

	if (__write_isolated_load_params(spe, addr_h, addr_l, size)) {
		errno = EIO;
		return -errno;
	}

	return 0;
}

/**
 * Load the emulated isolation loader program from the filesystem
 *
 * @return The loader program, or NULL if it can't be loaded. The loader binary
 *	   is cached between calls.
 */
static spe_program_handle_t *emulated_loader_program(void)
{
	static spe_program_handle_t *loader = NULL;

	if (!loader)
		loader = _base_spe_image_open(SPE_EMULATED_LOADER_FILE);

	if (!loader)
		DEBUG_PRINTF("Can't load emulated loader '%s': %s\n",
				SPE_EMULATED_LOADER_FILE, strerror(errno));

	return loader;
}

/**
 * Check if the emulated loader is present in the filesystem
 * @return Non-zero if the loader is available, otherwise zero.
 */
int _base_spe_emulated_loader_present(void)
{
	spe_program_handle_t *loader = emulated_loader_program();

	if (!loader)
		return 0;

	return !_base_spe_verify_spe_elf_image(loader);
}

/**
 * Initiate transfer of an emulated isolated SPE app by the loader kernel.
 *
 * Helper function for internal libspe use.
 *
 * @param thread The SPE thread to load the app to
 * @param handle The handle to the (isolated) spe program
 * @param ld_info[out] Loader information about the entry point of the SPE.
 *		This will reference the loader, not the SPE program, as
 *		we will be running the loader first.
 * @return zero on success, non-zero on failure;
 */
static int spe_start_emulated_isolated_app(struct spe_context *spe,
		spe_program_handle_t *handle, struct spe_ld_info *ld_info)

{
	int rc;
	spe_program_handle_t *loader;

	/* load emulated loader from the filesystem */
	loader = emulated_loader_program();

	if (!loader)
		return -1;

	rc = _base_spe_load_spe_elf(loader, spe->base_private->mem_mmap_base, ld_info);
	if (rc != 0) {
		DEBUG_PRINTF("%s: No loader available\n", __FUNCTION__);
		return rc;
	}

	return spe_start_isolated_app(spe, handle);
}

int _base_spe_program_load(spe_context_ptr_t spe, spe_program_handle_t *program)
{
	int rc = 0;
	struct spe_ld_info ld_info;

	spe->base_private->loaded_program = program;

	if (spe->base_private->flags & SPE_ISOLATE) {
		rc = spe_start_isolated_app(spe, program);

	} else if (spe->base_private->flags & SPE_ISOLATE_EMULATE) {
		rc = spe_start_emulated_isolated_app(spe, program, &ld_info);

	} else {
		rc = _base_spe_load_spe_elf(program,
				spe->base_private->mem_mmap_base, &ld_info);
		if (!rc)
			_base_spe_program_load_complete(spe);
	}

	if (rc != 0) {
		DEBUG_PRINTF ("Load SPE ELF failed..\n");
		return -1;
	}

	spe->base_private->entry = ld_info.entry;
	spe->base_private->emulated_entry = ld_info.entry;

	return 0;
}
