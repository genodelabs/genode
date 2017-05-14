#
# Prevent double definition of '__size_t' in 'glob/glob.h'
#
CPPFLAGS += -D__FreeBSD__

include $(call select_from_repositories,mk/noux.mk)
