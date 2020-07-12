/*
 * libspe - A wrapper library to adapt the JSRE SPU usage model to SPUFS
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

#include <stdlib.h>
#include <spu_intrinsics.h>

void delay(void)
{
	int i,j,a[128];
	
	for(i=0; i< 200000 ; i++)
	{
		for(j=0; j< 128; j++)
		{
			a[j] += i;
		}
		
	}
}


int main(void)
{
	unsigned int data;

	delay();

	/* send a first event */
	spu_writech(SPU_WrOutIntrMbox, 0xf00);

	/* wait for app to get it */
	data = spu_readch(SPU_RdInMbox);

	delay();

	/* send the second event that used to be broken */
	spu_writech(SPU_WrOutIntrMbox, 0xbaa);

	/* wait for signal to finish */
	data = spu_readch(SPU_RdInMbox);

	return 0;
}
