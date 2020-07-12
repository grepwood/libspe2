#include <stdio.h>
#include <spu_mfcio.h>
main()
{
	unsigned int data;
	printf("Spu says hello!\n");

	data = 24;
	spu_write_out_intr_mbox(data);
	while (spu_stat_out_intr_mbox() == 0) {
		printf(">> %d\n", spu_stat_out_intr_mbox());
	}

	return 42;
}
