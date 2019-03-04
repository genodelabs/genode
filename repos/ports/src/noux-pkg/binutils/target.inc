CFLAGS += -std=c99
CONFIGURE_ARGS = --disable-werror \
                 --program-prefix=$(PROGRAM_PREFIX) \
                 --target=$(BINUTILS_TARGET) \
                 --disable-nls

#
# Pass CFLAGS and friends to the invokation of 'make' because
# binutils execute 2nd-level configure scripts, which need
# the 'MKENV' as well.
#
MAKE_ENV = $(MKENV)

PKG_DIR = $(call select_from_ports,binutils)/src/noux-pkg/binutils

include $(call select_from_repositories,mk/noux.mk)
