AF_LIB = @AF_LIB@

bin_SCRIPTS = esd-config

EXTRA_DIST = \
	esd.m4 \
	EsounD.html \
	TIPS \
	test-script \
	test.wav \
	getopt.h

lib_LTLIBRARIES = libesd.la

libesd_la_SOURCES = \
	esdlib.c	\
	audio.c

libesd_la_LDFLAGS = -version-info 2:5:2

EXTRA_libesd_la_SOURCES = \
	audio_aix.c	\
	audio_hpux.c	\
	audio_irix.c	\
	audio_none.c	\
	audio_oss.c	\
	audio_mklinux.c	\
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
	esdloop \
	@ESDPLAY@

EXTRA_PROGRAMS = \
	esdplay

esd_SOURCES = 	\
	esd.c 		\
	clients.c	\
	mix.c		\
	players.c	\
	proto.c		\
	samples.c	\
	esd-server.h

esdcat_SOURCES = \
	esdcat.c

esdctl_SOURCES = \
	esdctl.c

esdmon_SOURCES = \
	esdmon.c

esdrec_SOURCES = \
	esdrec.c

esdsample_SOURCES = \
	esdsample.c

esdloop_SOURCES = \
	esdloop.c

EXTRA_esdplay_SOURCES = \
	esdplay.c

LDADD = libesd.la -lm

esdplay_LDADD = @LIBOBJS@ $(AF_LIB) $(LDADD)

m4datadir = $(datadir)/aclocal
m4data_DATA = esd.m4
