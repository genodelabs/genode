CFLAGS += -std=c99
CONFIGURE_ARGS = --disable-werror \
                 --program-prefix=$(PROGRAM_PREFIX) \
                 --target=$(BINUTILS_TARGET)

#
# Pass CFLAGS and friends to the invokation of 'make' because
# binutils execute 2nd-level configure scripts, which need
# the 'ENV' as well.
#
MAKE_ENV = $(ENV)

PKG_DIR = $(call select_from_ports,binutils)/src/noux-pkg/binutils

include $(REP_DIR)/mk/noux.mk
