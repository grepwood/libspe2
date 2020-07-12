
#include <stdlib.h>
#include "libspe2.h"

int main()
{
	spe_context_ptr_t ctx;
	unsigned int flags = 0;
	unsigned int entry = SPE_DEFAULT_ENTRY;
	void * argp = NULL;
	void * envp = NULL;
	spe_program_handle_t * program;
	
	program = spe_image_open("hello");

	ctx = spe_context_create(flags, 0);
	spe_program_load(ctx, program);
	spe_context_run(ctx, &entry, flags, argp, envp, NULL);
	spe_context_destroy(ctx);
}

