NOUX_CONFIGURE_ARGS = --disable-glibtest --disable-largefile \
					  --disable-mouse --disable-extra --disable-nls \
					  --exec-prefix= --bindir=/bin --sbindir=/bin \

NOUX_CFLAGS += -U__BSD_VISIBLE

LIBS += ncurses


#
# Make the ncurses linking test succeed
#
Makefile: dummy_libs

NOUX_LDFLAGS += -L$(PWD)

.SECONDARY: dummy_libs
dummy_libs: libncurses.a

libncurses.a:
	$(VERBOSE)$(AR) -rc $@

include $(REP_DIR)/mk/noux.mk
