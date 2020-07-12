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

CFLAGS += -I$(TOP)
CFLAGS += -I$(TOP)/spebase

LDFLAGS := $(CFLAGS)
LDLIBS  := -L$(TOP) -lspe2 

elfspe_OBJS := elfspe.o

all:  elfspe

elfspe: $(elfspe_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(TOP)/$(libspe2_SO)

clean:
	rm -f $(elfspe_OBJS) elfspe

install: elfspe elfspe-register
	$(INSTALL_DIR)	   $(ROOT)$(bindir)
	$(INSTALL_PROGRAM) elfspe	            $(ROOT)$(bindir)/elfspe
	$(INSTALL_PROGRAM) elfspe-register      $(ROOT)$(bindir)/elfspe-register
	$(INSTALL_PROGRAM) scripts/elfspe-unregister    $(ROOT)$(bindir)/elfspe-unregister

#
# when building in the cross environment this script should reference the
# boot-time relative location not the sysroot relative location.
#
elfspe-register: scripts/elfspe-register
	sed -e "s%@BINDIR@%${bindir}%g" <$< >$@
	chmod +x $@
