#! /bin/sh
aclocal $ACLOCAL_FLAGS
autoheader
automake --gnu --add-missing
autoconf
./configure $*
