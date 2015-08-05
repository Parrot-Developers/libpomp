#!/bin/sh

# Run this to generate all the initial makefiles, etc.

package=libpomp

if [ "$1" = "--clean" ]; then
	rm -f configure config.h.in aclocal.m4 INSTALL
	find . -name Makefile.in -delete
	rm -f m4/libtool.m4 m4/lt~obsolete.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4
	rm -rf build-aux
	exit 0
fi

autoreconf --force --install --verbose
rm -rf autom4te.cache
rm -f config.h.in~
echo ""
echo "type './configure' to configure $package."
echo "type 'make' to compile $package."
echo "type 'make install' to install $package."
