#
# ident	@(#)Makefile.cmd 1.3	02/04/04 GED
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
