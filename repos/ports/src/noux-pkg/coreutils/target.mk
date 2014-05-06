NOUX_CONFIGURE_ARGS = --disable-acl --disable-largefile --disable-xattr \
                      --disable-libcap --disable-nls

#
# Prevent building stdbuf because this involves the linkage of a shared
# libary, which is not supported by Noux, yet.
#
NOUX_CPPFLAGS += -U__ELF__
NOUX_MAKE_ENV += "MAKEINFO=true"

include $(REP_DIR)/mk/noux.mk
