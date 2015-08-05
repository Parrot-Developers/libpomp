
ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = src

if BUILD_TOOLS
SUBDIRS += tools
endif

if BUILD_EXAMPLES
SUBDIRS += examples
endif

EXTRA_DIST = \
	libpomp.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libpomp.pc
