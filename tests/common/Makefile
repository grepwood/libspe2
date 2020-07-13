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

.PHONY: all check clean distclean

all: $(ppu_utils)

$(ppu_utils): $(TEST_CONFIG)

$(TEST_CONFIG)::
	@-$(RM) $@.tmp
	@echo "#define NUM_ITERATIONS $(TEST_ITERATION)" >> $@.tmp
	@echo "#define NUM_SPES $(TEST_SPES)" >> $@.tmp
	@[ "$(TEST_AFFINITY)" != "1" ] || echo "#define ENABLE_AFFINITY $(TEST_AFFINITY)" >> $@.tmp
	@[ "$(TEST_STATIC)" != "1" ] || echo "#define ENABLE_STATIC $(TEST_STATIC)" >> $@.tmp
	@[ "$(TEST_VERBOSE)" != "1" ] || echo "#define ENABLE_VERBOSE $(TEST_VERBOSE)" >> $@.tmp
	@diff $@ $@.tmp >/dev/null 2>/dev/null || cp $@.tmp $@
	@-$(RM) $@.tmp

check:

clean:
	-$(RM) $(TEST_CONFIG)
	-$(RM) *.elf *.o *.tmp

distclean: clean
	-$(RM) *~
