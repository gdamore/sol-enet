#
# ident	"@(#)Makefile	1.6	02/05/21 GED"
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
	( cd $@; $(MAKE) ISA=$(ISA) $(TARGET));

all:		$(SUBDIRS)
install:	$(SUBDIRS)
lint:		$(SUBDIRS)
clean:		$(SUBDIRS)
clobber:	$(SUBDIRS)

include $(TOP)/Makefile.targ

FRC:
