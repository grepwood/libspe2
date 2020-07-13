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
	test_c99_stdio_string.sh \
	test_c99_stdio_stdin_stdout.sh \
	test_c99_stdio_file.sh \
	test_c99_stdio_file_stdin_stdout.sh \
	test_c99_stdio_file_stdin_stderr.sh \
	test_c99_stdio_gets_incomplete_line.sh \
	test_c99_stdio_misc.sh

ppu_progs = \
	test_c99_main.elf

spu_progs = \
	spu_c99_stdio_string.spu.elf \
	spu_c99_stdio_stdin_stdout.spu.elf \
	spu_c99_stdio_file.spu.elf \
	spu_c99_stdio_file_stdin_stdout.spu.elf \
	spu_c99_stdio_file_stdin_stderr.spu.elf \
	spu_c99_stdio_gets_incomplete_line.spu.elf \
	spu_c99_stdio_misc.spu.elf


include $(TEST_TOP)/make.rules

$(spu_progs:.spu.elf=.spu.o): spu_c99_test.h

spu_c99_stdio_file.spu.elf: spu_c99_stdio_file_common.spu.o

spu_c99_stdio_file_stdin_stdout.spu.elf: spu_c99_stdio_file_common.spu.o

spu_c99_stdio_file_stdin_stderr.spu.elf: spu_c99_stdio_file_common.spu.o
