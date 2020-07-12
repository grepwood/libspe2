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


CFLAGS += -I$(TOP)/spebase
CFLAGS += -I$(TOP)/spethread
CFLAGS += -I$(TOP)/spegang
CFLAGS += -I$(TOP)/speevent
CFLAGS += -I$(TOP) -g

LDFLAGS := $(CFLAGS)
LDLIBS := -L$(TOP) -lspe2 -lpthread

ALL := testsingle testsinglemini hello hellointr\
		testmap1 \
		testpthread testpthreadmini \
		testlibspe2 testlibspe2hello \
		testinfo testhandler \
		testevent1 testevent2 \
		testmbox readmbox

all: $(ALL)

hello: helloworld.c
	spu-gcc -o hello helloworld.c

hellointr: hellointr.c
	spu-gcc -o hellointr hellointr.c

hellointr: readmbox.c
	spu-gcc -o readmbox readmbox.c

testlibspe2hello: testlibspe2hello.c
	spu-gcc -o testlibspe2hello testlibspe2hello.c


clean:
	rm -f $(ALL)

