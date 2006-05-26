#
# $Id: Makefile,v 1.3 2006/05/26 18:17:23 gdamore Exp $
#
# Top-level Makefile for drivers.
#

TOP=..
include  ../Makefile.common

all		:=	TARGET=all
install		:=	TARGET=install
lint		:=	TARGET=lint
ddict		:=	TARGET=ddict
clean		:=	TARGET=clean
clobber		:=	TARGET=clobber

SUBDIRS		=	afe mxfe
DISTSRCS	=	Makefile.common Makefile.drv Makefile

$(SUBDIRS):	FRC
	@echo "## Making $(TARGET) for $(ISA) in drv/$@"; \
	( cd $@; $(MAKE) ISA=$(ISA) DEBUG=$(DEBUG) RELDIR=drv/$@ $(TARGET));

all:		$(SUBDIRS)
install:	$(SUBDIRS)
lint:		$(SUBDIRS)
ddict:		$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)

include $(TOP)/Makefile.targ

FRC:
