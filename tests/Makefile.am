noinst_PROGRAMS =

noinst_PROGRAMS += tst-pomp
tst_pomp_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src
tst_pomp_CFLAGS = $(CUNIT_CFLAGS)
tst_pomp_LDADD = $(top_builddir)/src/libpomp.la $(CUNIT_LIBS)
tst_pomp_LDFLAGS =

if !OS_WIN32
tst_pomp_LDFLAGS += -pthread
endif

tst_pomp_SOURCES = pomp_test.c \
	pomp_test_addr.c \
	pomp_test_basic.c \
	pomp_test_ctx.c \
	pomp_test_loop.c \
	pomp_test_ipc.c \
	pomp_test_timer.c
