
DOCS = @DOCS@

doc: $(DOCS)

esound/index.html: esound.sgml
	@DB2HTML@ $(srcdir)/esound.sgml
	cp esound/book1.html esound/index.html

esound.ps: esound.sgml
	cd $(srcdir) && @DB2PS@ esound.sgml

distclean: clean
	rm -rf ./esound
	rm -f $(srcdir)/esound.dvi $(srcdir)/esound.ps $(srcdir)/esound.tex $(srcdir)/esound.aux $(srcdir)/esound.log

clean:
