#*
#* libspe - A wrapper library to adapt the JSRE SPU usage model to SPUFS
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


CC := gcc
SPECC := spu-gcc

CFLAGS := -O2 -Wall -I../.. -I../../include -g
SPECFLAGS := -O2 -Wall -I../../include
SPELDFLAGS := -L/usr/spu/lib

LIBS := -L../.. -lspe -lpthread
SPELIBS := -lc -lgloss

SPE_OBJS := spe-helloworld
OBJS := runspe

all: $(SPE_OBJS) $(OBJS)

clean:
	rm -f $(SPE_OBJS) $(OBJS)

$(SPE_OBJS): %: %.c
	$(SPECC) $(SPECFLAGS) $(SPELDFLAGS) -o $@ $< $(SPELIBS)

$(OBJS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)
