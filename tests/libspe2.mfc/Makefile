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
	test_wbox.elf \
	test_ibox.elf \
	test_mbox.elf \
	test_mbox_all.elf \
	test_mbox_error.elf \
	test_wbox_simultaneous.elf \
	test_ibox_simultaneous.elf \
	test_mbox_simultaneous.elf \
	test_signal.elf \
	test_signal_error.elf \
	test_proxy_dma.elf \
	test_ibox_stop.elf

extra_main_progs = \
	test_proxy_dma_poll.elf \
	test_dma.elf \
	test_dma_page_fault.elf \
	test_dma_stop.elf


include $(TEST_TOP)/make.rules

test_wbox.elf: spu_wbox.embed.o

test_ibox.elf: spu_ibox.embed.o

test_mbox.elf: spu_mbox.embed.o

test_mbox_all.elf: spu_mbox_all.embed.o

test_wbox_simultaneous.elf: spu_wbox.embed.o

test_ibox_simultaneous.elf: spu_ibox.embed.o

test_mbox_simultaneous.elf: spu_mbox.embed.o

test_signal.elf: spu_signal.embed.o

test_proxy_dma.elf: spu_proxy_dma.embed.o

test_ibox_stop.elf: spu_ibox_stop.embed.o

test_proxy_dma_poll.elf: spu_proxy_dma.embed.o

test_dma.elf: spu_dma.embed.o

test_dma_page_fault.elf: spu_dma.embed.o

test_dma_stop.elf: spu_dma_stop.embed.o
