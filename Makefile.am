DIST_SUBDIRS = m4 t

EXTRA_DIST = 

SUBDIRS = . t

ACLOCAL_AMFLAGS = -I m4

# noinst_LTLIBRARIES = librff.la

bin_PROGRAMS = rfile_fuse

# librff_la_SOURCES = utils.h utils.c buffer.h buffer.c
# librff_la_LDFLAGS = -avoid-version -static

rfile_fuse_SOURCES = rfile_fuse.c
# rfile_fuse_LDADD = librff.la
rfile_fuse_CFLAGS = $(libfuse_CFLAGS)
rfile_fuse_LDFLAGS = $(libfuse_LIBS)

test: all
	cd t && $(MAKE) test

