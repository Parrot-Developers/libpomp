
AC_PREREQ([2.68])

AC_INIT([libpomp],
	[1.0.0],
	[yves-marie.morgan@parrot.com])

AC_CONFIG_SRCDIR([libpomp.pc.in])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability])

AC_SUBST(VERSION)

AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

dnl Check for Win32
AC_MSG_CHECKING([for Win32])
case "$host" in
	*-*-mingw*)
		os_win32=yes
	;;
	*)
		os_win32=no
	;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, [test $os_win32 = yes])

dnl building of tools
AC_ARG_ENABLE(tools,
	AS_HELP_STRING([--disable-tools],[disable building tools]),
	[
		case "${enableval}" in
		yes) BUILD_TOOLS=yes ;;
		no)  BUILD_TOOLS=no ;;
		*)   AC_MSG_ERROR(bad value ${enableval} for --disable-tools) ;;
		esac
	],
	[BUILD_TOOLS=yes]
)
AM_CONDITIONAL(BUILD_TOOLS, test "x$BUILD_TOOLS" = "xyes")

dnl building of examples
AC_ARG_ENABLE(examples,
	AS_HELP_STRING([--enable-examples],[enable building examples]),
	[
		case "${enableval}" in
		yes) BUILD_EXAMPLES=yes ;;
		no)  BUILD_EXAMPLES=no ;;
		*)   AC_MSG_ERROR(bad value ${enableval} for --enable-examples) ;;
		esac
	],
	[BUILD_EXAMPLES=no]
)
AM_CONDITIONAL(BUILD_EXAMPLES, test "x$BUILD_EXAMPLES" = "xyes")

dnl building of tests
AC_ARG_ENABLE(tests,
	AS_HELP_STRING([--enable-tests],[enable building tests]),
	[
		case "${enableval}" in
		yes) BUILD_TESTS=yes ;;
		no)  BUILD_TESTS=no ;;
		*)   AC_MSG_ERROR(bad value ${enableval} for --enable-tests) ;;
		esac
	],
	[BUILD_TESTS=no]
)
AM_CONDITIONAL(BUILD_TESTS, test "x$BUILD_TESTS" = "xyes")

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL

AX_CXX_COMPILE_STDCXX_11([],[optional])
AM_CONDITIONAL(HAVE_CXX11, test "$HAVE_CXX11" = "1")

AC_HEADER_STDC

AC_CHECK_HEADERS([ \
	inttypes.h \
	netdb.h \
	unistd.h \
	sys/epoll.h \
	sys/event.h \
	sys/eventfd.h \
	sys/param.h \
	sys/poll.h \
	sys/socket.h \
	sys/timerfd.h \
	sys/un.h \
	netinet/tcp.h \
])

dnl Check for POSIX timers
AC_CHECK_FUNCS(timer_create, [], [
	AC_CHECK_LIB(rt, timer_create, [
		AC_DEFINE(HAVE_TIMER_CREATE, 1)
		LIBS="$LIBS -lrt"
		], [
		AC_CHECK_LIB(pthread, timer_create, [
			AC_DEFINE(HAVE_TIMER_CREATE, 1)
			LIBS="$LIBS -lpthread"
		])
	])
])

dnl Check for cunit if test are enabled
AS_IF([test "x$BUILD_TESTS" = "xyes"], [
	PKG_CHECK_MODULES([CUNIT], [cunit])
	AC_SUBST(CUNIT_CFLAGS)
	AC_SUBST(CUNIT_LIBS)
])

LT_INIT

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile 
	src/Makefile
	examples/Makefile
	tests/Makefile
	tools/Makefile
	libpomp.pc
])
AC_OUTPUT
