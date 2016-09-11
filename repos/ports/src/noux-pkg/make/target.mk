#
# Prevent double definition of '__size_t' in 'glob/glob.h'
#
CPPFLAGS += -D__FreeBSD__

include $(REP_DIR)/mk/noux.mk
