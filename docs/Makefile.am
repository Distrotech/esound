
DOCS = @DOCS@

doc: $(DOCS)

esound/index.html: esound.sgml
	@DB2HTML@ esound.sgml && cp esound/book1.html esound/index.html

esound.ps: esound.sgml
	@DB2PS@ esound.sgml

distclean: clean
	rm -rf ./esound
	rm -f esound.dvi esound.ps esound.tex esound.aux esound.log

clean:
