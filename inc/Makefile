#
# $Id: Makefile,v 1.2 2004/08/27 23:39:34 gdamore Exp $
#
# inc/Makefile
#

TOP = ..

include  ../Makefile.common

HDRS		=	sys/afeimpl.h \
			sys/afe.h \
			sys/mii.h \
			sys/mxfeimpl.h \
			sys/mxfe.h

DISTSRCS	=	$(HDRS) Makefile

# we may wish to actually deliver the afe.h header file at a later date...

all:		$(HDRS)
install:	$(HDRS)
clean:
clobber:

include $(TOP)/Makefile.targ

FRC:
