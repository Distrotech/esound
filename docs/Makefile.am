man_MANS = esd.1 esdcat.1 esd-config.1 esdctl.1 esddsp.1 esdfilt.1 esdloop.1 esdmon.1 esdplay.1 esdrec.1 esdsample.1 

EXTRA_DIST = esd.1.in esdcat.1.in esd-config.1.in esdctl.1.in esddsp.1.in esdfilt.1.in esdloop.1.in esdmon.1.in esdplay.1.in esdrec.1.in esdsample.1.in 

DOCS = @DOCS@

doc: $(DOCS)

esound/index.html: esound.sgml
	@DB2HTML@ $(srcdir)/esound.sgml

esound.ps: esound.sgml
	cd $(srcdir) && @DB2PS@ esound.sgml

distclean: clean
	rm -rf $(srcdir)/esound $(srcdir)/esound.junk
	rm -f $(srcdir)/esound.dvi $(srcdir)/esound.ps $(srcdir)/esound.tex $(srcdir)/esound.aux $(srcdir)/esound.log

clean:
