EXTRA_DIST = \
EsounD.html \
TIPS \
test-script \
test.wav

lib_LTLIBRARIES = libesd.la

libesd_la_SOURCES = \
esdlib.c	\
audio.c

EXTRA_libesd_la_SOURCES = \
audio_aix.c	\
audio_hpux.c	\
audio_irix.c	\
audio_none.c	\
audio_oss.c	\
audio_solaris.c	

include_HEADERS = \
esd.h

bin_PROGRAMS = \
esd \
esdcat \
esdctl \
esdmon \
esdrec \
esdsample \
esdloop

esd_SOURCES = 	\
esd.c 		\
clients.c	\
mix.c		\
players.c	\
proto.c		\
samples.c	\
esd-server.h

#esd_LDADD = libesd.la

esdcat_SOURCES = \
esdcat.c

#esdcat_LDADD = libesd.la

esdctl_SOURCES = \
esdctl.c

#esdctl_LDADD = libesd.la

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

LDADD = -lm libesd.la

