#
# $Id: Makefile,v 1.2 2004/08/27 22:58:49 gdamore Exp $
#
# cmd/etherdiag/Makefile
#

TOP=../..
include	../Makefile.common

PROGNAME	=	etherdiag
SRCS		=	etherdiag.c
HDRS		=	$(TOP)/inc/sys/afe.h
LDLIBS		=	-ldevinfo -lkstat
CPPFLAGS	+=	-D_REENTRANT
#
# libdevinfo is not lint friendly
#
LINTFLAGS	=	-erroff=E_NAME_DECL_NOT_USED_DEF2
LINTFLAGS	+=	-erroff=E_NAME_USED_NOT_DEF2

include	../Makefile.cmd
