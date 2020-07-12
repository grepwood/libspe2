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

TOP=.

include $(TOP)/make.defines

libspe2_OBJS := libspe2.o

CFLAGS += -I$(TOP)/spebase
CFLAGS += -I$(TOP)/speevent


all:  $(libspe2_SO) $(libspe2_A) libspe12-all

dist:  pushall all

pushall:
	quilt push -a ; true

popall:
	quilt pop -a ; true

base-all: 
	$(MAKE) -C spebase all

event-all: 
	$(MAKE) -C speevent all

elfspe-all:
	$(MAKE) -C elfspe all

libspe12-all:
	$(MAKE) -C libspe12 all


install: libspe12-install
	$(INSTALL_DIR)     $(ROOT)$(libdir)
	$(INSTALL_DATA)	   $(libspe2_A)			$(ROOT)$(libdir)/$(libspe2_A)
	$(INSTALL_PROGRAM) $(libspe2_SO)		$(ROOT)$(libdir)/$(libspe2_SO)
	$(INSTALL_LINK)	   $(libspe2_SO)		$(ROOT)$(libdir)/$(libspe2_SONAME)
	$(INSTALL_LINK)	   $(libspe2_SONAME)	$(ROOT)$(libdir)/libspe2.so
	$(INSTALL_DIR)     $(ROOT)$(includedir)
	$(INSTALL_DATA)    libspe2.h            $(ROOT)$(includedir)/libspe2.h
	$(INSTALL_DATA)    libspe2-types.h      $(ROOT)$(includedir)/libspe2-types.h
	$(INSTALL_DIR)	   $(ROOT)$(includedir)
	$(INSTALL_DATA)	   spebase/cbea_map.h	$(ROOT)$(includedir)/cbea_map.h
	$(INSTALL_DIR)     $(ROOT)$(speinclude)
	$(INSTALL_DATA)	   spebase/cbea_map.h   $(ROOT)$(speinclude)/cbea_map.h

elfspe-install:
	$(MAKE) -C elfspe install

libspe12-install:
	$(MAKE) -C libspe12 install

tests: tests/Makefile
	make -C tests

tags:
	$(CTAGS) -R .

$(libspe2_SO): $(libspe2_OBJS) base-all  event-all 
	$(CC) $(CFLAGS) -shared -o $@ $(libspe2_OBJS) spebase/*.o speevent/*.o -lrt -lpthread -Wl,--soname=${libspe2_SONAME}

$(libspe2_A): $(libspe2_OBJS) base-all  event-all 
	 $(CROSS)ar -r $(libspe2_A) $(libspe2_OBJS) spebase/*.o speevent/*.o $(libspe2_OBJS)


PACKAGE		= libspe2
FULLNAME	= $(PACKAGE)-$(MAJOR_VERSION).$(MINOR_VERSION)
TARBALL		= $(SOURCES)$(FULLNAME).tar.gz
SOURCEFILES	= $(TARBALL)
PATCHES		:= `cat series | grep -v ^\#`
#RPMBUILD	= ppc32 rpmbuild --target=ppc 
RPMBUILD	= rpmbuild 

PWD	:= $(shell pwd)
RPM	:= $(PWD)/..
SOURCES = $(PWD)/
RPMS	= $(RPM)/RPMS/
SRPMS	= $(RPM)/SRPMS/
BUILD	= $(RPM)/BUILD/
TMP	= $(RPM)/tmp/

SPEC	 ?= $(PACKAGE).spec
RPMBUILD ?= rpmbuild

checkenv:
	@if [ ""x == "$(PACKAGE)"x \
	  -o ""x == "$(SOURCEFILES)"x \
	  -o ""x == "$(SPEC)"x ] ; then \
	  	echo "inconsistant make file" false; fi

$(PWD)/.rpmmacros:
	mkdir -p $(SOURCES) $(RPMS) $(SRPMS) $(BUILD)
	echo -e \%_topdir $(RPM)\\n\%_sourcedir $(PWD)\\n\%_tmppath %_topdir/tmp > $@

rpm: checkenv $(RPM)/$(PACKAGE)-stamp

$(RPM)/$(PACKAGE)-stamp: $(PWD)/.rpmmacros $(SOURCEFILES) $(SPEC)
	HOME=$(PWD) $(RPMBUILD) -ba $(SPEC) $(RPMFLAGS)
	touch $@

crossrpm: checkenv $(RPM)/$(PACKAGE)-cross-stamp

$(RPM)/$(PACKAGE)-cross-stamp: $(PWD)/.rpmmacros $(SOURCEFILES) $(SPEC)
	HOME=$(PWD) $(RPMBUILD) -ba $(SPEC) --target=noarch $(RPMFLAGS)
	touch $@

.PHONY: checkenv rpm crossrpm


rpms: clean rpm rpm32

rpm32: 
	HOME=$(PWD) $(RPMBUILD) --target=ppc -ba $(SPEC) $(RPMFLAGS)
rpm64: 
	HOME=$(PWD) $(RPMBUILD) --target=ppc64 -ba $(SPEC) $(RPMFLAGS)



tarball: $(TARBALL)

$(TARBALL): $(SOURCES)
#	cd patches; cp $(PATCHES) ..
	rm -f $(FULLNAME)
	ln -s . $(FULLNAME)
	tar czf $@ --exclude=$(FULLNAME).tar.gz \
		--exclude=$(FULLNAME)/$(FULLNAME) \
		--exclude=$(FULLNAME)/spegang \
		--exclude=$(FULLNAME)/spethread \
		--exclude=$(FULLNAME)/tests_hidden \
		--exclude=$(FULLNAME)/patches \
		--exclude=CVS $(FULLNAME)/*

doc: text pdf

text: xml
	cd doc; xsltproc extractfunctions.xslt ../xml/all.xml > functions.txt

xml: clean
	doxygen doc/DoxyfileRef
	cd xml; xsltproc combine.xslt index.xml >all.xml

pdf: apiref detail

detail: clean
	doxygen doc/Doxyfile
	cd latex; make; cp refman.pdf ../doc/detail.pdf

apiref: clean
	cd doc/img; make
	doxygen doc/DoxyfileRef
	cp doc/img/*.pdf latex
	cd latex; make; cp refman.pdf ../doc/apiref.pdf



clean: base-clean event-clean elfspe-clean libspe12-clean
	rm *.diff ; true
	rm -rf $(libspe2_A) $(libspe2_SO) $(libspe2_OBJS)
	rm -f $(TARBALL)
	rm -f doc/*.pdf
	rm -f doc/functions.txt
	rm -rf xml
	rm -rf html
	rm -rf latex
	make -C tests clean

base-clean: 
	$(MAKE) -C spebase clean

event-clean: 
	$(MAKE) -C speevent clean

elfspe-clean: 
	$(MAKE) -C elfspe clean

libspe12-clean:
	$(MAKE) -C libspe12 clean


distclean: clean
	rm -rf latex
	rm -rf html

.PHONY: all clean tests tags
