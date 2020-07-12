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

PACKAGE		:= libspe2
VERSION		:= $(MAJOR_VERSION).$(MINOR_VERSION)
RELEASE		:= $(shell svnversion)
FULLNAME	:= $(PACKAGE)-$(VERSION)
PACKAGE_VER	:= $(FULLNAME)-$(RELEASE)
TARBALL		:= $(SOURCES)$(PACKAGE_VER).tar.gz
SOURCEFILES	:= $(TARBALL)

edit = @sed \
	-e 's,@prefix@,$(prefix),g' \
	-e 's,@exec_prefix@,$(exec_prefix),g' \
	-e 's,@libdir@,$(libdir),g' \
	-e 's,@name@,$(PACKAGE),g' \
	-e 's,@includedir@,$(includedir),g' \
	-e 's,@version@,$(VERSION),g'

all:  $(libspe2_SO) $(libspe2_A) libspe12-all

dist:  $(TARBALL)

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
	$(INSTALL_DIR) 	   $(ROOT)$(libdir)/pkgconfig
	$(INSTALL_DATA)	   $(libspe2_A)			$(ROOT)$(libdir)/$(libspe2_A)
	$(INSTALL_PROGRAM) $(libspe2_SO)		$(ROOT)$(libdir)/$(libspe2_SO)
	$(INSTALL_LINK)	   $(libspe2_SO)		$(ROOT)$(libdir)/$(libspe2_SONAME)
	$(INSTALL_DATA)    libspe2.pc           $(ROOT)$(libdir)/pkgconfig
	$(INSTALL_LINK)	   $(libspe2_SONAME)	$(ROOT)$(libdir)/libspe2.so
	$(INSTALL_DIR)     $(ROOT)$(includedir)
	$(INSTALL_DATA)    libspe2.h            $(ROOT)$(includedir)/libspe2.h
	$(INSTALL_DATA)    libspe2-types.h      $(ROOT)$(includedir)/libspe2-types.h
	$(INSTALL_DIR)	   $(ROOT)$(includedir)
	$(INSTALL_DATA)	   spebase/cbea_map.h	$(ROOT)$(includedir)/cbea_map.h
	$(INSTALL_DIR)     $(ROOT)$(speinclude)
	$(INSTALL_DATA)	   spebase/cbea_map.h   $(ROOT)$(speinclude)/cbea_map.h
	$(INSTALL_DIR)     $(ROOT)$(adabindingdir)
	$(INSTALL_DATA)    ada/README $(ROOT)$(adabindingdir)/README-libspe2
	$(INSTALL_DATA)    ada/cbea_map.ads $(ROOT)$(adabindingdir)/cbea_map.ads
	$(INSTALL_DATA)    ada/libspe2.ads  $(ROOT)$(adabindingdir)/libspe2.ads
	$(INSTALL_DATA)    ada/libspe2_types.ads $(ROOT)$(adabindingdir)/libspe2_types.ads

elfspe-install:
	$(MAKE) -C elfspe install

libspe12-install:
	$(MAKE) -C libspe12 install

tests: tests/Makefile
	make -C tests

ada : ada/Makefile
	make -C ada

tags:
	$(CTAGS) -R .

libspe2.pc: Makefile $(TOP)/libspe2.pc.in
	@rm -f $@ $@.tmp
	$(edit) $(TOP)/$@.in >$@.tmp
	@mv $@.tmp $@

$(libspe2_SO): $(libspe2_OBJS) base-all  event-all libspe2.pc 
	$(CC) $(CFLAGS) -shared -o $@ $(libspe2_OBJS) spebase/*.o speevent/*.o -lrt -lpthread -Wl,--soname=${libspe2_SONAME}

$(libspe2_A): $(libspe2_OBJS) base-all  event-all 
	 $(CROSS)ar -r $(libspe2_A) $(libspe2_OBJS) spebase/*.o speevent/*.o $(libspe2_OBJS)


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
	@if [ ""x = "$(PACKAGE)"x \
	  -o ""x = "$(SOURCEFILES)"x \
	  -o ""x = "$(SPEC)"x ] ; then \
		echo "inconsistant make file" false; fi

$(PWD)/.rpmmacros:
	mkdir -p $(SOURCES) $(RPMS) $(SRPMS) $(BUILD)
	echo -e \%_topdir $(RPM)\\n\%_sourcedir $(PWD)\\n\%_tmppath %_topdir/tmp\\n\%_version $(RELEASE) > $@

rpm: dist checkenv $(RPM)/$(PACKAGE)-stamp

$(RPM)/$(PACKAGE)-stamp: $(PWD)/.rpmmacros $(SOURCEFILES) $(SPEC)
	HOME=$(PWD) $(RPMBUILD) -ba $(SPEC) $(RPMFLAGS)
	touch $@

crossrpm: dist checkenv $(RPM)/$(PACKAGE)-cross-stamp

$(RPM)/$(PACKAGE)-cross-stamp: $(PWD)/.rpmmacros $(SOURCEFILES) $(SPEC)
	HOME=$(PWD) $(RPMBUILD) -ba $(SPEC) --target=noarch $(RPMFLAGS)
	touch $@

.PHONY: checkenv rpm crossrpm


rpms: clean rpm rpm32

rpm32: 
	HOME=$(PWD) $(RPMBUILD) --target=ppc -ba $(SPEC) $(RPMFLAGS)
rpm64: 
	HOME=$(PWD) $(RPMBUILD) --target=ppc64 -ba $(SPEC) $(RPMFLAGS)

$(TARBALL): clean
	ln -s . $(FULLNAME)
	tar -czhf $@ --exclude=$(PACKAGE_VER).tar.gz \
			--exclude=$(FULLNAME)/$(FULLNAME) \
			--exclude=CVS --exclude .pc --exclude '.*.swp' --exclude '*~'  --exclude '.#*' \
			--exclude=$(FULLNAME)/spegang \
			--exclude=$(FULLNAME)/spethread \
			--exclude=$(FULLNAME)/tests_hidden \
			--exclude=$(FULLNAME)/patches \
			--exclude=.svn \
			$(FULLNAME) 
	rm $(FULLNAME)

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
	rm -f $(FULLNAME)
	rm -f doc/*.pdf
	rm -f doc/functions.txt
	rm -rf xml
	rm -rf html
	rm -rf latex
	rm -f libspe2.pc
	make -C tests clean
	make -C ada clean


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
