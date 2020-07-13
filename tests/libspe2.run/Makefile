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
	test_cpu_info.elf \
	test_direct_access.elf \
	test_single_context.elf \
	test_run_image.elf \
	test_stop.elf \
	test_multiple_context.elf \
	test_nosched_context.elf \
	test_single_gang_context.elf \
	test_context_create_error.elf \
	test_run_error.elf \
	test_image_error.elf \
	test_ppe_assisted_call.elf

ifeq ($(TEST_AFFINITY),1)
main_progs += \
	test_affinity.elf \
	test_affinity_error.elf
endif

ppu_progs =

spu_progs = \
	spu_arg.spu.elf \
	spu_null.spu.elf \
	spu_spin.spu.elf \
	spu_hello.spu.elf \
	spu_non_exec.spu.elf \
	spu_non_elf.spu.elf


include $(TEST_TOP)/make.rules

test_single_context.elf: spu_exit.embed.o spu_arg.embed.o

test_stop.elf: spu_stop.embed.o

test_multiple_context.elf: spu_arg.embed.o

test_nosched_context.elf: spu_arg.embed.o

test_single_gang_context.elf: spu_arg.embed.o

test_affinity.elf: spu_arg.embed.o

test_affinity_error.elf: spu_null.embed.o

test_run_error.elf: spu_halt.embed.o spu_invalid_instr.embed.o \
	spu_invalid_channel.embed.o spu_dma_error.embed.o spu_invalid_dma.embed.o

test_ppe_assisted_call.elf: spu_ppe_assisted_call.embed.o

spu_non_exec.spu.elf: spu_null.spu.elf
	cp $< $@.tmp
	chmod -x $@.tmp
	mv $@.tmp $@

spu_non_elf.spu.elf:
	dd if=/dev/zero of=$@.tmp bs=1048576 count=16
	chmod 755 $@.tmp
	mv $@.tmp $@
