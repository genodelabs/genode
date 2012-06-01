NOUX_CONFIGURE_ARGS = --with-ssl \
                      --with-zlib \
                      --disable-nls \
                      --disable-ipv6 \
                      --disable-rpath-hack \
                      --with-cfg-file=/etc/lynx.cfg \
                      --with-lss-file=/etc/lynx.lss

#
# Needed for <sys/types.h>
#
NOUX_CFLAGS += -D__BSD_VISIBLE

LIBS += ncurses zlib libssl libc_resolv

#
# Make the zlib linking test succeed
#
Makefile: dummy_libs 

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libcrypto.a libssl.a libz.a

libcrypto.a:
	$(VERBOSE)$(AR) -rc $@

libssl.a:
	$(VERBOSE)$(AR) -rc $@

libz.a:
	$(VERBOSE)$(AR) -rc $@


include $(REP_DIR)/mk/noux.mk
