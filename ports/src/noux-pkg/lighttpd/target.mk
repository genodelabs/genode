TARGET = lighttpd

NOUX_CONFIGURE_ARGS += --disable-ipv6 \
                       --disable-mmap \
                       --without-bzip2 \
                       --without-pcre

LIBS += libcrypto libssl zlib #libc_resolv

#
# Make the zlib linking test succeed
#
Makefile: dummy_libs

NOUX_LDFLAGS += -L$(PWD)

dummy_libs: libz.a libcrypto.a libssl.a

libcrypto.a:
	$(VERBOSE)$(AR) -rc $@
libssl.a:
	$(VERBOSE)$(AR) -rc $@
libz.a:
	$(VERBOSE)$(AR) -rc $@

include $(REP_DIR)/mk/noux.mk
