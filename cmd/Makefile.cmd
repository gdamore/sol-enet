#
# $Id: Makefile.cmd,v 1.2 2004/08/27 22:59:23 gdamore Exp $
#
# cmd/Makefile.cmd
#

all:	$(HDRS) $(SRCS) .WAIT $(PROG)

lint:	$(HDRS)	$(SRCS) .WAIT $(LINTS)
	$(LINT) $(LINTFLAGS) $(LINTS) $(LDLIBS)

clean:
	$(RM) $(OBJS) $(LINTS)

clobber:
	$(RM) $(PROG)

$(PROG):	$(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(TOP)/obj.$(ISA)/%.o:	%.c
	$(CC) -c -o $@ $(CFLAGS) $(CPPFLAGS) $<

$(TOP)/obj.$(ISA)/%.ln:	%.c
	$(LINT) -c -dirout=$(TOP)/obj.$(ISA) $(LINTFLAGS) $(CPPFLAGS) $<

include $(TOP)/Makefile.targ
