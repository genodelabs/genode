CONFIGURE_ARGS = --disable-acl --disable-largefile --disable-xattr \
                 --disable-libcap --disable-nls

#
# Prevent building stdbuf because this involves the linkage of a shared
# libary, which is not supported by Noux, yet.
#
CPPFLAGS += -U__ELF__
MAKE_ENV += "MAKEINFO=true"

include $(REP_DIR)/mk/noux.mk
