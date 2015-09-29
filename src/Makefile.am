
lib_LTLIBRARIES = libpomp.la

libpomp_la_CPPFLAGS = -I$(top_srcdir)/include
libpomp_la_CFLAGS = -DPOMP_API_EXPORTS
libpomp_la_LDFLAGS = -no-undefined
libpomp_la_LIBADD = $(LIBS)

if !BUILD_TESTS
libpomp_la_CFLAGS += -fvisibility=hidden
endif

if OS_WIN32
libpomp_la_LIBADD += -lws2_32
endif

libpomp_la_SOURCES = \
	pomp_addr.c \
	pomp_buffer.c \
	pomp_conn.c \
	pomp_ctx.c \
	pomp_decoder.c \
	pomp_encoder.c \
	pomp_log.c \
	pomp_loop.c \
	pomp_msg.c \
	pomp_prot.c \
	pomp_timer.c

pkginclude_HEADERS = \
	$(top_srcdir)/include/libpomp.h \
	$(top_srcdir)/include/libpomp.hpp \
	$(top_srcdir)/include/libpomp-cxx11.hpp
