NOUX_CONFIGURE_FLAGS = --without-included-regex

#NOUX_INSTALL_TARGET =

LIBS += pcre

#
# Prevent double definition of '__size_t' in 'glob/glob.h'
#
#NOUX_CPPFLAGS += -D__FreeBSD__

include $(REP_DIR)/mk/noux.mk
