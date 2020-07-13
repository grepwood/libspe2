#
#  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
#
#  Copyright (C) 2008 Sony Computer Entertainment Inc.
#  Copyright 2008 Sony Corp.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

TEST_TOP = ..

include $(TEST_TOP)/make.defines

main_progs = \
	test_event_stop.elf \
	test_event_ibox.elf \
	test_event_tag_group.elf \
	test_event_wbox.elf \
	test_event.elf \
	test_event_error.elf


include $(TEST_TOP)/make.rules

test_event_stop.elf: spu_event_stop.embed.o

test_event_ibox.elf: spu_ibox.embed.o

test_event_tag_group.elf: spu_proxy_dma.embed.o

test_event_wbox.elf: spu_event_wbox.embed.o

test_event.elf: spu_event.embed.o

spu_ibox.c: ../libspe2.mfc/spu_ibox.c
	ln -sf $< $@

spu_proxy_dma.c: ../libspe2.mfc/spu_proxy_dma.c
	ln -sf $< $@
