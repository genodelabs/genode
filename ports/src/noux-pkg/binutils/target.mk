PROGRAM_PREFIX = genode-x86-

NOUX_CFLAGS += -std=c99
NOUX_CONFIGURE_ARGS = --disable-werror \
                      --program-prefix=$(PROGRAM_PREFIX) \
                      --target=i686-freebsd

#
# Pass CFLAGS and friends to the invokation of 'make' because
# binutils execute 2nd-level configure scripts, which need
# the 'NOUX_ENV' as well.
#
NOUX_MAKE_ENV = $(NOUX_ENV)

include $(REP_DIR)/mk/noux.mk
