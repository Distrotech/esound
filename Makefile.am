
SUBDIRS = docs

SPEC_FILE = esound.spec

INCLUDES = $(AUDIOFILE_CFLAGS) -DSYSCONFDIR=\"$(sysconfdir)\"

if BUILD_ESDDSP
esddsp = esddsp
libesddsp_la = libesddsp.la
endif
if BUILD_ESDPLAY
esdplay = esdplay
endif

getopt_extra = getopt.c getopt1.c

if NEED_GETOPT
getopt_src = $(getopt_extra)
endif

bin_SCRIPTS = \
	$(esddsp)	\
	esd-config

lib_LTLIBRARIES = libesd.la $(libesddsp_la)

libesd_la_LDFLAGS = -version-info $(ESD_VERSION_INFO)
libesd_la_LIBADD = $(AUDIOFILE_LIBS) $(SOUND_LIBS)

libesd_la_SOURCES = \
	esdlib.c	\
	esdmgr.c	\
	esdfile.c	\
	esd_config.c	\
	audio.c		\
	genrand.c	\
	genrand.h

# one of these gets included by audio.c
AUDIO_BACKENDS = \
	audio_aix.c	\
	audio_alsa.c	\
	audio_alsa09.c  \
	audio_hpux.c	\
	audio_irix.c	\
	audio_none.c	\
	audio_oss.c	\
	audio_mklinux.c	\
	audio_solaris.c	

libesddsp_la_LDFLAGS = -version-info $(ESD_VERSION_INFO)
libesddsp_la_LIBADD = $(DL_LIB) -lm

libesddsp_la_SOURCES = \
	esddsp.c


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
	$(esdplay)

esd_SOURCES = 	\
	esd.c 		\
	clients.c	\
	filter.c	\
	mix.c		\
	players.c	\
	proto.c		\
	samples.c	\
	$(getopt_src)   \
	esd-server.h

esdcat_SOURCES = \
	esdcat.c

esdctl_SOURCES = \
	$(getopt_src) \
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

esdplay_SOURCES = \
	$(getopt_src) \
	esdplay.c

LDADD = libesd.la $(AUDIOFILE_LIBS)

esd_LDADD = $(WRAP_LIB) $(LDADD)

m4datadir = $(datadir)/aclocal
m4data_DATA = esd.m4

esdconfdir = $(sysconfdir)
esdconf_DATA = esd.conf

$(PACKAGE)-$(VERSION).tar.gz: dist

rpm: $(PACKAGE)-$(VERSION).tar.gz
	cp $(PACKAGE)-$(VERSION).tar.gz /usr/src/redhat/SOURCES
	rpm -ba $(SPEC_FILE)

doc:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  test "$$subdir" = . || (cd $$subdir && $(MAKE) doc); \
	done


dist-hook: doc esound.spec
	test -d $(distdir)/docs || mkdir $(distdir)/docs 
	cp $(srcdir)/docs/esound.sgml $(distdir)/docs
	-cp -r $(srcdir)/docs/esound $(distdir)/docs/html
	-cp $(srcdir)/docs/esound.ps $(distdir)/docs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = esound.pc

EXTRA_DIST = \
	$(getopt_extra)	\
	esddsp		\
	esound.pc.in	\
	esound.spec	\
	esound.spec.in	\
	esd-config.in	\
        esd.conf        \
	esd.m4 		\
	TIPS 		\
	test-script 	\
	test.wav 	\
	getopt.h 	\
	TODO		\
	MAINTAINERS	\
	$(AUDIO_BACKENDS)
