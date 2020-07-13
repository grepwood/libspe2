#
#  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
#
#  Copyright (C) 2008 Sony Computer Entertainment Inc.
#  Copyright 2007,2008 Sony Corp.
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

TEST_TOP = .

include $(TEST_TOP)/make.defines

SUBDIRS = common $(addprefix libspe2.,$(test_sets))

# list of test sets
test_sets = run mfc c99 event basic


checks = $(addprefix check-,$(test_sets))

.PHONY: all check clean distclean help

all:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d $@ || exit 1; \
	done

check:
	@passed=""; \
	failed=""; \
	for s in $(test_sets); do \
		if $(MAKE) -s check-$$s; then \
			passed="$$passed $$s"; \
		else \
			failed="$$failed $$s"; \
		fi; \
		echo; \
	done; \
	echo "PASS:$$passed"; \
	echo "FAIL:$$failed"; \
	[ -z "$$failed" ]

$(checks):
	$(MAKE) -C libspe2.$(patsubst check-%,%,$@) check

clean:
	@-$(RM) *.tmp
	@-for d in $(SUBDIRS); do \
		$(MAKE) -C $$d $@; \
	done; \

distclean: clean
	@-$(RM) *~
	@-for d in $(SUBDIRS); do \
		$(MAKE) -C $$d $@; \
	done; \

help:
	@echo; \
	echo "Syntax:"; \
	echo "    $$ $(MAKE) [<TEST-PARAMETERS>] [<TEST-CASE-OR-TEST-SET>]"; \
	echo; \
	echo "Examples:"; \
	echo "    $$ $(MAKE) check"; \
	echo "    $$ $(MAKE) ARCH=ppc64 check"; \
	echo; \
	echo "Test parameters:"; \
	echo "    CROSS=<ppu-toolchain>     Prefix of PPU toolchain. (default: $(CROSS))"; \
	echo "    SPU_CROSS=<spu-toolchain> Prefix of SPU toolchain. (default: $(SPU_CROSS))"; \
	echo "    ARCH=<ppc-or-ppc64>       Whether libspe2 is 32-bit or 64-bit. (default: $(ARCH))"; \
	echo "    LIBSPE2_INCLUDE=<dir>     Use header files in the directory <dir>. (default: $(LIBSPE2_INCLUDE))"; \
	echo "    LIBSPE2_LIB=<dir>         Use library files in the directory <dir>. (default: $(LIBSPE2_LIB))"; \
	echo "    TEST_STATIC=1             Use static libraries instead of shared libraries."; \
	echo "    TEST_SPES=<n>             Number of SPEs to be used. (default: $(TEST_SPES))"; \
	echo "                              Some tests may ignore this parameter."; \
	echo "    TEST_ITERATION=<n>        Run each test <n> times repeatedly (default: $(TEST_ITERATION))."; \
	echo "                              Some tests may ignore this parameter."; \
	echo "    TEST_AFFINITY=1           Enable affinity tests (default: $(TEST_AFFINITY))."; \
	echo "    TEST_VERBOSE=1            Display many information while testing."; \
	echo; \
	echo "Test sets:"; \
	echo "    check                     Run all tests."; \
	for t in $(test_sets); \
	do \
		echo "    check-$$t"; \
	done; \
	echo
