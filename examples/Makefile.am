
noinst_PROGRAMS =

noinst_PROGRAMS += pomp-ping
pomp_ping_CPPFLAGS = -I$(top_srcdir)/include
pomp_ping_LDADD = $(top_builddir)/src/libpomp.la
pomp_ping_SOURCES = ping.c

if HAVE_CXX11
noinst_PROGRAMS += pomp-ping-cpp
pomp_ping_cpp_CPPFLAGS = -I$(top_srcdir)/include
pomp_ping_cpp_LDADD = $(top_builddir)/src/libpomp.la
pomp_ping_cpp_SOURCES = ping.cpp
endif
