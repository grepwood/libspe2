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
#include <stdlib.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <unistd.h>

#include "elf_loader.h"
#include "spebase.h"

struct image_handle {
	spe_program_handle_t speh;
	unsigned int map_size;
};

spe_program_handle_t *_base_spe_image_open(const char *filename)
{
	/* allocate an extra integer in the spe handle to keep the mapped size information */
	struct image_handle *ret;
	int binfd = -1, f_stat;
	struct stat statbuf;
	size_t ps = getpagesize ();

	ret = malloc(sizeof(struct image_handle));
	if (!ret)
		return NULL;

	ret->speh.handle_size = sizeof(spe_program_handle_t);
	ret->speh.toe_shadow = NULL;

	binfd = open(filename, O_RDONLY);
	if (binfd < 0)
		goto ret_err;

	f_stat = fstat(binfd, &statbuf);
	if (f_stat < 0)
		goto ret_err;

	/* Sanity: is it executable ?
	 */
	if(!(statbuf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
		errno=EACCES;
		goto ret_err;
	}
	
	/* now store the size at the extra allocated space */
	ret->map_size = (statbuf.st_size + ps - 1) & ~(ps - 1);

	ret->speh.elf_image = mmap(NULL, ret->map_size,
							PROT_WRITE | PROT_READ,
							MAP_PRIVATE, binfd, 0);
	if (ret->speh.elf_image == MAP_FAILED)
		goto ret_err;

	/*Verify that this is a valid SPE ELF object*/
	if((verify_spe_elf_image((spe_program_handle_t *)ret)))
		goto ret_err;

	if (toe_ear(&ret->speh))
		goto ret_err;

	/* ok */
	close(binfd);
	return (spe_program_handle_t *)ret;

	/* err & cleanup */
ret_err:
	if (binfd >= 0)
		close(binfd);

	free(ret);
	return NULL;
}

int _base_spe_image_close(spe_program_handle_t *handle)
{
	int ret = 0;
	struct image_handle *ih;

	if (!handle) {
		errno = EINVAL;
		return -1;
	}
	
	ih = (struct image_handle *)handle;

	if (!ih->speh.elf_image || !ih->map_size) {
		errno = EINVAL;
		return -1;
	}

	if (ih->speh.toe_shadow)
		free(ih->speh.toe_shadow);

	ret = munmap(ih->speh.elf_image, ih->map_size );
	free(handle);

	return ret;
}


