#
# ident	"@(#)Makefile	1.10	02/05/21 GED"
#
# man/Makefile
#

TOP = ..

include  ../Makefile.common

MAN7DS		=	afe.7d mxfe.7d
MAN1MS		=	etherdiag.1m
MANS		=	$(MAN7DS) $(MAN1MS)
POSTSCRIPTS	=	$(MAN7DS:%.7d=%.ps) $(MAN1MS:%.1m=%.ps)
TEXTS		=	$(MAN7DS:%.7d=%.txt) $(MAN1MS:%.1m=%.txt)
DISTSRCS	=	$(MANS:%=%.in) Makefile

# we may wish to actually deliver the man pages at a later date...

all:		$(MANS) $(POSTSCRIPTS) $(TEXTS)
install:	$(MANS) $(POSTSCRIPTS) $(TEXTS)
clean:
		$(RM) $(POSTSCRIPTS) $(TEXTS)
clobber: clean
		$(RM) $(MANS)

include $(TOP)/Makefile.targ

%: %.in ../Makefile.common
	sed -e 's/%VERSION%/$(VER)/' -e 's/%PKG%/$(PKG)/' < $< >$@

%.ps: %.1m
	$(SGML2ROFF) $< | $(TROFF) -man -Tpost - | $(DPOST) > $@
%.ps: %.7d
	$(SGML2ROFF) $< | $(TROFF) -man -Tpost - | $(DPOST) > $@

%.txt: %.1m
	$(SGML2ROFF) $< | $(NROFF) -man > $@
%.txt: %.7d
	$(SGML2ROFF) $< | $(NROFF) -man > $@

FRC:
