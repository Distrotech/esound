AF_LIB = @AF_LIB@

bin_SCRIPTS = esd-config

EXTRA_DIST = \
	esd.m4 		\
	EsounD.html 	\
	TIPS 		\
	test-script 	\
	test.wav 	\
	getopt.h 	\
	TODO		\
	esddsp.c

lib_LTLIBRARIES = libesd.la

libesd_la_SOURCES = \
	esdlib.c	\
	audio.c

libesd_la_LDFLAGS = -version-info @ESD_VERSION_INFO@

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
	esd		\
	esdcat		\
	esdctl		\
	esdfilt		\
	esdmon		\
	esdrec		\
	esdsample	\
	esdloop		\
	@ESDPLAY@

EXTRA_PROGRAMS = \
	esdplay

esd_SOURCES = 	\
	esd.c 		\
	clients.c	\
	filter.c	\
	mix.c		\
	players.c	\
	proto.c		\
	samples.c	\
	esd-server.h

esdcat_SOURCES = \
	esdcat.c

esdctl_SOURCES = \
	esdctl.c

esdfilt_SOURCES = \
	esdfilt.c

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

LIBOBJS = @LIBOBJS@

esdctl_LDADD = $(LIBOBJS) $(LDADD)
esdplay_LDADD = $(LIBOBJS) $(AF_LIB) $(LDADD)

m4datadir = $(datadir)/aclocal
m4data_DATA = esd.m4
