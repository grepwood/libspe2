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

MAJOR_VERSION	:= 2
MINOR_VERSION	:= 0.0

libspebase_A	:= libspebase.a
libspebase_SO	:= libspebase.so.${MAJOR_VERSION}.${MINOR_VERSION}
libspebase_SONAME	:= libspebase.so.${MAJOR_VERSION}

libspebase_OBJS := create.o  elf_loader.o load.o run.o image.o lib_builtin.o \
				default_c99_handler.o default_posix1_handler.o dma.o mbox.o \
				accessors.o info.o

CFLAGS += -I..
CFLAGS += -D_ATFILE_SOURCE

all: $(libspebase_SO) $(libspebase_A)

install-so: $(libspebase_SO) $(libspebase_A)
	$(INSTALL_DIR)	   $(ROOT)$(includedir)
	$(INSTALL_DATA)	   spebase.h	        $(ROOT)$(includedir)/spebase.h
	$(INSTALL_DATA)	   cbea_map.h	        $(ROOT)$(includedir)/cbea_map.h
	$(INSTALL_DIR)	   $(ROOT)$(speinclude)
	$(INSTALL_DATA)	   cbea_map.h	        $(ROOT)$(speinclude)/cbea_map.h

clean:
	rm -rf $(libspebase_A) $(libspebase_OBJS) $(libspebase_SO)

$(libspebase_SO): $(libspebase_OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $^ -lrt -lpthread -Wl,--soname=${libspebase_SONAME}

$(libspebase_A): $(libspebase_OBJS)
	 $(CROSS)ar -r $(libspebase_A) $(libspebase_OBJS)


