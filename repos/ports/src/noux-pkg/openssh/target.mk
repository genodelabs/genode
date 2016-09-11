TARGET = openssh

#
# This prefix 'magic' is needed because OpenSSH uses $exec_prefix
# while compiling (e.g. -DSSH_PATH) and in the end the $prefix and
# $exec_prefix path differ.
#
CONFIGURE_ARGS += --disable-ip6 \
                  --with-md5-passwords  \
                  --without-zlib-version-check \
                  --with-ssl-engine \
                  --disable-finger \
                  --disable-gopher \
                  --disable-news \
                  --disable-ftp \
                  --disable-rpath-hack \
                  --disable-utmpx \
                  --disable-strip \
                  --exec-prefix= \
                  --bindir=/bin \
                  --sbindir=/bin \
                  --libexecdir=/bin

INSTALL_TARGET  = install

LIBS += libcrypto libssl zlib libc_resolv

built.tag: Makefile Makefile_patch

Makefile_patch: Makefile
	@#
	@# Our $(LDFLAGS) contain options which are usable by gcc(1)
	@# only. So instead of using ld(1) to link the binary, we have
	@# to use gcc(1).
	@#
	$(VERBOSE)sed -i 's|^LD=.*|LD=$(CC)|' Makefile
	@#
	@# We do not want to generate host-keys because we are crosscompiling
	@# and we can not run Genode binaries on the build system.
	@#
	$(VERBOSE)sed -i 's|^install:.*||' Makefile
	$(VERBOSE)sed -i 's|^install-nokeys:|install:|' Makefile
	@#
	@# The path of ssh(1) is hardcoded to $(bindir)/ssh which in our
	@# case is insufficient.
	@#
	$(VERBOSE)sed -i 's|^SSH_PROGRAM=.*|SSH_PROGRAM=/bin/ssh|' Makefile


#
# Make the zlib linking test succeed
#
Makefile: dummy_libs

LDFLAGS += -L$(PWD)

dummy_libs: libz.a libcrypto.a libssl.a

libcrypto.a:
	$(VERBOSE)$(AR) -rc $@
libssl.a:
	$(VERBOSE)$(AR) -rc $@
libz.a:
	$(VERBOSE)$(AR) -rc $@

include $(REP_DIR)/mk/noux.mk
