
TOP=..

include $(TOP)/make.defines

CFLAGS += -I$(TOP)
CFLAGS += -I$(TOP)/spebase
CFLAGS += -D_ATFILE_SOURCE

LDFLAGS := -m32
LDLIBS := -L$(TOP) -lspe2 -lpthread

MAJOR_VERSION	:= 1
MINOR_VERSION	:= 2.1

libspe12_A	:= libspe.a
libspe12_SO	:= libspe.so.${MAJOR_VERSION}.${MINOR_VERSION}
libspe12_SONAME	:= libspe.so.${MAJOR_VERSION}

libspe12_OBJS := spe.o spethreads.o dma.o ps.o ../spebase/*.o

all:  $(libspe12_SO) $(libspe12_A)

$(libspe12_A): $(libspe12_OBJS)
	 ar -r $(libspe12_A) $(libspe12_OBJS)

$(libspe12_SO): $(libspe12_OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $^ -lrt -Wl,--soname=${libspe12_SONAME}

install: spe.h $(libspe12_SO) $(libspe12_A)
	$(INSTALL_DIR)     $(ROOT)$(libdir)
	$(INSTALL_DATA)	   $(libspe12_A)	    $(ROOT)$(libdir)/$(libspe12_A)
	$(INSTALL_PROGRAM) $(libspe12_SO)	    $(ROOT)$(libdir)/$(libspe12_SO)
	$(INSTALL_LINK)	   $(libspe12_SO)	    $(ROOT)$(libdir)/$(libspe12_SONAME)
	$(INSTALL_LINK)	   $(libspe12_SONAME) 	$(ROOT)$(libdir)/libspe.so
	$(INSTALL_DIR)	   $(ROOT)$(includedir)
	$(INSTALL_DATA)	   libspe.h	        $(ROOT)$(includedir)/libspe.h


clean:
	rm -f $(libspe12_SONAME) $(libspe12_OBJS) $(libspe12_A) 
	
