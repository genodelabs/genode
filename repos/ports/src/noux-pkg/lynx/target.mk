CONFIGURE_ARGS = --with-ssl \
                 --with-zlib \
                 --disable-nls \
                 --disable-ipv6 \
                 --disable-rpath-hack \
                 --with-cfg-file=/etc/lynx.cfg \
                 --with-lss-file=/etc/lynx.lss

#
# Rather than dealing with autoconf force usage of <openssl/xxx.h>
# by defining it explicitly
#
CFLAGS += -DUSE_OPENSSL_INCL

#
# Needed for <sys/types.h>
#
CFLAGS += -D__BSD_VISIBLE

LIBS += ncurses zlib libssl libcrypto libc_resolv

#
# Make the zlib linking test succeed
#
Makefile: dummy_libs

LDFLAGS += -L$(PWD)

dummy_libs: libcrypto.a libssl.a libz.a

libcrypto.a:
	$(VERBOSE)$(AR) -rc $@

libssl.a:
	$(VERBOSE)$(AR) -rc $@

libz.a:
	$(VERBOSE)$(AR) -rc $@

INSTALL_TARGET = install

include $(REP_DIR)/mk/noux.mk
