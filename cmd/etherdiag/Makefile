#
# ident	@(#)Makefile 1.15	03/05/19 GED
#
# cmd/etherdiag/Makefile
#

TOP=../..
include	../Makefile.common

PROGNAME	=	etherdiag
SRCS		=	etherdiag.c
HDRS		=	$(TOP)/inc/sys/afe.h
LDLIBS		=	-ldevinfo -lkstat
#
# libdevinfo is not lint friendly
#
LINTFLAGS	=	-erroff=E_NAME_DECL_NOT_USED_DEF2
LINTFLAGS	+=	-erroff=E_NAME_USED_NOT_DEF2

include	../Makefile.cmd
