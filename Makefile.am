AF_LIB = @AF_LIB@
LIBESDDSP = @LIBESDDSP@

bin_SCRIPTS = \
	@ESDDSP@	\
	esd-config

EXTRA_SCRIPTS = esddsp

EXTRA_DIST = \
	esd.m4 		\
	EsounD.ps 	\
	TIPS 		\
	test-script 	\
	test.wav 	\
	getopt.h 	\
	TODO		\
	esound.spec

lib_LTLIBRARIES = libesd.la @LIBESDDSP@

libesd_la_SOURCES = \
	esdlib.c	\
	esdmgr.c	\
	audio.c

libesd_la_LDFLAGS = -version-info @ESD_VERSION_INFO@

EXTRA_LTLIBRARIES = \
	libesddsp.la

libesddsp_la_LDFLAGS = -rpath $(libdir) -version-info @ESD_VERSION_INFO@

libesddsp_la_LIBADD = -lm

libesddsp_la_SOURCES = \
	esddsp.c

EXTRA_libesd_la_SOURCES = \
	audio_aix.c	\
	audio_alsa.c	\
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

esd_LDADD = $(LIBOBJS) $(WRAP_LIB) $(LDADD)
esdctl_LDADD = $(LIBOBJS) $(LDADD)
esdplay_LDADD = $(LIBOBJS) $(AF_LIB) $(LDADD)

m4datadir = $(datadir)/aclocal
m4data_DATA = esd.m4
