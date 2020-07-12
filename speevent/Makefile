#*
#* libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS 
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

libspeevent_A	:= libspeevent.a
libspeevent_SO	:= libspeevent.so.${MAJOR_VERSION}.${MINOR_VERSION}
libspeevent_SONAME	:= libspeevent.so.${MAJOR_VERSION}
libspeevent_OBJS := spe_event.o

CFLAGS += -I$(TOP)/spebase
CFLAGS += -I$(TOP)

all: $(libspeevent_SO) $(libspeevent_A)

install-so: $(libspeevent_SO) $(libspeevent_A)
	$(INSTALL_DIR)	   $(ROOT)$(includedir)
	$(INSTALL_DATA)	   speevent.h	        $(ROOT)$(includedir)/speevent.h
	$(INSTALL_DIR)	   $(ROOT)$(speinclude)

clean:
	rm -f $(libspeevent_OBJS)
	rm -f $(libspeevent_SO)
	rm -f libspeevent.a

$(libspeevent_SO): $(libspeevent_OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $^ -lrt -lpthread -Wl,--soname=${libspeevent_SONAME}

$(libspeevent_A): $(libspeevent_OBJS)
	 $(CROSS)ar -r $(libspeevent_A) $(libspeevent_OBJS) ../spebase/*.o


