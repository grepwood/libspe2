

#include <stdio.h>

#include "libspe2.h"

int main(void)
{
	int no, nocpus , i;

	nocpus  = spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, -2);
	printf("-2 ==> %d (%d)\n", nocpus, errno);

	nocpus  = spe_cpu_info_get(SPE_COUNT_PHYSICAL_CPU_NODES, -1);
	printf("-1 ==> %d (%d)\n", nocpus, errno);

	for (i=-2; i <= nocpus; i++) {
		no = spe_cpu_info_get(SPE_COUNT_PHYSICAL_SPES, i);
		printf("%2d ==> %d (%d)\n",i, no, errno);
		
		no = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, i);
		printf("%2d ==> %d (%d)\n",i, no, errno);
	}
}

