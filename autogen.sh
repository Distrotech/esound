#! /bin/sh
aclocal $ACLOCAL_FLAGS
automake
autoconf
./configure $*
