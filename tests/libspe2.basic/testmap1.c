#include <stdio.h>
#include <stdlib.h>
#include "libspe2.h"

int main(void)
{
	spe_context_ptr_t ctx;
	int flags = SPE_MAP_PS;
	struct spe_mfc_command_area * mfc_cmd_area;
	struct spe_spu_control_area * spu_control_area; 
	unsigned int MFC_LSA;
	unsigned int status;
	
	printf("starting ..\n");
	ctx = spe_context_create(flags, NULL);
	mfc_cmd_area = spe_ps_area_get(ctx, SPE_MFC_COMMAND_AREA);
	printf("mfc_cmd_area is: %p\n", mfc_cmd_area);
	MFC_LSA = mfc_cmd_area->MFC_LSA;
	spu_control_area = spe_ps_area_get(ctx, SPE_CONTROL_AREA);
	status = spu_control_area->SPU_Status;
	spe_context_destroy(ctx);
	printf("%d done\n", status);
	printf("%d done\n", MFC_LSA);

	return 0;
}
