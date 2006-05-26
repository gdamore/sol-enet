#
# $Id: Makefile,v 1.3 2006/05/26 18:17:23 gdamore Exp $
#
# cmd/Makefile
#

TOP=..

include  ../Makefile.common

all		:=	TARGET=all
install		:=	TARGET=install
lint		:=	TARGET=lint
clean		:=	TARGET=clean
clobber		:=	TARGET=clobber

SUBDIRS		=	etherdiag
DISTSRCS	=	Makefile.common Makefile.cmd Makefile

$(SUBDIRS):	FRC
	@echo "## Making $(TARGET) for $(ISA) in cmd/$@"; \
	( cd $@; $(MAKE) DEBUG=$(DEBUG) ISA=$(ISA) $(TARGET));

all:		$(SUBDIRS)
install:	$(SUBDIRS)
lint:		$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)

include $(TOP)/Makefile.targ

FRC:
