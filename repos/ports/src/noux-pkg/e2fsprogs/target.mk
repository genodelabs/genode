CONFIGURE_FLAGS = --disable-tls --enable-fsck

# NOTE: --sbindir=/bin is broken and the easist fix is patching configure
#       directly and therefore is not used.

#
# Needed for <sys/types.h>
#
CFLAGS += -D__BSD_VISIBLE

include $(REP_DIR)/mk/noux.mk
