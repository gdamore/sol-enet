#
# $Id: Makefile,v 1.2 2004/08/27 23:33:42 gdamore Exp $
#

TOP		= ..
POSTSCRIPTS	= $(XMLS:%.xml=%.ps)
PDFS		= $(XMLS:%.xml=%.pdf)
HTMLS		= $(XMLS:%.xml=%.html)
PHTMLS		= $(XMLS:%.xml=%.phtml)
XMLS		= GUIDE.xml
DISTSRCS	= Makefile $(XMLS) xml2html.pl XMLLite.pm note.gif warn.gif

include ../Makefile.common

all:	pdfs postscripts htmls

pdfs:	$(PDFS)

htmls:	$(HTMLS)

postscripts:	$(POSTSCRIPTS)

clean:
	$(RM) $(PHTMLS)

clobber: clean
	$(RM) $(PDFS) $(HTMLS) $(POSTSCRIPTS)

%.phtml:	%.xml
	$(RM) $@
	./xml2html.pl -v $(VER) < $< > $@

%.html:	%.phtml
	$(HTMLDOC) --bodycolor white -f $@ $<

%.pdf:	%.phtml
	$(HTMLDOC) -f $@ $<

%.ps:	%.phtml
	$(HTMLDOC) -f $@ $<

include ../Makefile.targ
