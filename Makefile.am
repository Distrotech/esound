EXTRA_DIST = \
EsounD.html \
TIPS

lib_LTLIBRARIES = libesd.la

libesd_la_SOURCES = \
esdlib.c	\
audio.c

noinst_LIBRARIES = libesd.a

libesd_a_SOURCES = \
esdlib.c	\
audio.c

include_HEADERS = \
esd.h

bin_PROGRAMS = \
esd \
esdcat \
esdlock \
esdunlock \
esdmon \
esdrec \
esdsample \
esdloop

esd_SOURCES = 	\
esd.c 		\
clients.c	\
mix.c		\
players.c	\
samples.c	

#esd_LDADD = libesd.la

esdcat_SOURCES = \
esdcat.c

#esdcat_LDADD = libesd.la

esdlock_SOURCES = \
esdlock.c

#esdlock_LDADD = libesd.la

esdunlock_SOURCES = \
esdunlock.c

#esdunlock_LDADD = libesd.la

esdmon_SOURCES = \
esdmon.c

#esdmon_LDADD = libesd.la

esdrec_SOURCES = \
esdrec.c

#esdrec_LDADD = libesd.la

esdsample_SOURCES = \
esdsample.c

#esdsample_LDADD = libesd.la

esdloop_SOURCES = \
esdloop.c

#esdloop_LDADD = libesd.la

LDADD = -lm .libs/libesd.so

