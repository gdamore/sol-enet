#
# ident	@(#)Makefile 1.9	02/05/21 GED
#
# cmd/Makefile
#

TOP = ..

include  ../Makefile.common

all		:=	TARGET=all
install		:=	TARGET=install
clean		:=	TARGET=clean
clobber		:=	TARGET=clobber

SUBDIRS		=	GEDenetd GEDenetm GEDenetu
COMMONSRCS	=	copyright
DISTSRCS	=	copyright Makefile Makefile.common Makefile.pkg

$(SUBDIRS):	FRC
	@echo "## Making $(TARGET) for $(ISA) in drv/$@"; \
	( cd $@; $(MAKE) ISA=$(ISA) $(TARGET));

all:		$(COMMONSRCS) .WAIT $(SUBDIRS)
install:	$(COMMONSRCS) .WAIT $(SUBDIRS)
clean:		$(COMMONSRCS) .WAIT $(SUBDIRS)
clobber:	$(COMMONSRCS) .WAIT $(SUBDIRS)

include $(TOP)/Makefile.targ

FRC:
