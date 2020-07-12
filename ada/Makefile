#*
#* libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS 
#* Copyright (C) 2005 IBM Corp. 
#*
#* This library is free software; you can redistribute it and/or modify it
#* under the terms of the GNU Lesser General Public License as published by 
#* the Free Software Foundation; either version 2.1 of the License, 
#* or (at your option) any later version.
#*
#*  This library is distributed in the hope that it will be useful, but 
#*  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
#*  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public 
#*  License for more details.
#*
#*   You should have received a copy of the GNU Lesser General Public License 
#*   along with this library; if not, write to the Free Software Foundation, 
#*   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#*

TOP=..

include $(TOP)/make.defines

adabindings := libspe2 libspe2_types cbea_map\
		testsingle	
spu_images := hello

all: adabinding tests spu_programs

adabinding : libspe2.ads libspe2_types.ads cbea_map.ads
	$(CC) -c cbea_map.ads 
	$(CC) -c libspe2.ads
	$(CC) -c libspe2_types.ads

tests : testsingle.adb 
	$(CROSS)gnatmake -g $^

spu_programs:  hello 

hello: helloworld.c
	$(SPU_CC) $(SPU_CFLAGS) $^ -o $@


install: $(adabinding) 

clean:
	rm -f $(adabindings) 
	rm -f $(spu_images)
	rm -f *.o *.ali b~*

