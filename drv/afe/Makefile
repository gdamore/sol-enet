#
# $Id: Makefile,v 1.2 2004/08/27 23:02:29 gdamore Exp $
#
# drv/afe/Makefile
#

TOP=../..

include	../Makefile.common

MODNAME		=	afe
SRCS		=	afe.c
HDRS		=	$(TOP)/inc/sys/afe.h \
			$(TOP)/inc/sys/afeimpl.h \
			$(TOP)/inc/sys/mii.h
LDFLAGS		+=	-dy -Nmisc/gld

include ../Makefile.drv
