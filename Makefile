#
# $Id: Makefile,v 1.4 2005/11/28 17:34:25 gdamore Exp $
#
# Makefile
#

TOP		=	.

include  Makefile.common
DISTNAME	=	etherdrivers

all		:=	TARGET=all
install		:=	TARGET=install
lint		:=	TARGET=lint
ddict		:=	TARGET=ddict
clean		:=	TARGET=clean
clobber		:=	TARGET=clobber
distsrcs	:=	TARGET=distsrcs

sparc_ARCHS	=	sparc sparcv9
i386_ARCHS	=	i386 amd64
ARCH		=	$(HOST_ARCH:-%=%)
ARCHS_LIST	=	$(HOST_ARCH:-%=%_ARCHS)
ARCHS		=	${${ARCHS_LIST}}

SUBDIRS		=	inc drv cmd pkg man doc

DISTSRCS	=	Makefile Makefile.common Makefile.targ \
			README TODO COPYING CHANGES COMPILING PERFORMANCE

DISTLIST	=	$(MAKE) RELDIR=$(DISTNAME)-$(VER) distlist

all:		inc .WAIT drv cmd man
install:	all .WAIT inc drv cmd man
lint:		drv cmd
ddict:		drv
clean:		inc drv cmd man pkg doc
distsrcs:	inc drv cmd man pkg doc
docs:		doc

clobber:	inc drv cmd man pkg doc
		$(RM) -f $(DISTNAME)*.zip

include Makefile.targ

dist:	distsrc .WAIT distbin

distsrc:	$(DISTNAME)-$(VER)-src.zip

distbin:	$(DISTNAME)-$(VER)-$(ARCH).zip

distclean:	clean
	$(RM) $(DISTNAME)-$(VER)-src.zip $(DISTNAME)-$(VER)-sparc.zip \
	$(DISTNAME)-$(VER)-i386.zip

$(DISTNAME)-$(VER)-$(ARCH).zip:	package docs
	@$(RM) $(DISTNAME)-$(VER)
	mkdir $(DISTNAME)-$(VER)
	mkdir $(DISTNAME)-$(VER)/Packages
	mkdir $(DISTNAME)-$(VER)/Packages/$(ARCH)
	cp README $(DISTNAME)-$(VER)
	cp COPYING $(DISTNAME)-$(VER)
	cp doc/GUIDE.html $(DISTNAME)-$(VER)
	cp doc/GUIDE.pdf $(DISTNAME)-$(VER)
	cp doc/GUIDE.ps $(DISTNAME)-$(VER)
	pkgtrans -o pkgdir.$(ARCH) $(DISTNAME)-$(VER)/Packages/$(ARCH) all
	zip -r $@ $(DISTNAME)-$(VER)
	@$(RM) -r $(DISTNAME)-$(VER)

$(DISTNAME)-$(VER)-src.zip:	distsrcs
	@$(RM) $@
	@$(RM) $(DISTNAME)-$(VER)
	@ln -s . $(DISTNAME)-$(VER)
	@zip $@ $(DISTLIST:sh)
	@$(RM) $(DISTNAME)-$(VER)

$(SUBDIRS):	FRC
	@for ISA in $(ARCHS); do	\
		echo "## Making $(TARGET) for $$ISA in $@"; \
		test -d obj.$$ISA || $(MKDIR) obj.$$ISA;		\
		( cd $@; $(MAKE) ISA=$$ISA $(TARGET));	\
	done

package:	all
	cd pkg; $(MAKE) ISA=$$ISA all

FRC:
