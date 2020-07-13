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
	testsingle.elf \
	testsinglemini.elf \
	testpthread.elf \
	testpthreadmini.elf \
	testlibspe2.elf \
	testinfo.elf \
	testhandler.elf \
	testevent1.elf \
	testevent2.elf \
	testmbox.elf

ppu_progs =

spu_progs = \
	helloworld.spu.elf \
	hellointr.spu.elf \
	testlibspe2hello.spu.elf \
	readmbox.spu.elf

extra_main_progs = \
	testmap1.elf

include $(TEST_TOP)/make.rules
