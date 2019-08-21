CONFIGURE_ARGS = --disable-acl --disable-largefile --disable-xattr \
                 --disable-libcap --disable-nls

#
# libstdbuf.so does not link on ARMv8
#
CONFIGURE_ARGS += --enable-no-install-program=stdbuf

#
# Prevent building stdbuf because this involves the linkage of a shared
# libary, which is not supported by Noux, yet.
#
CPPFLAGS += -U__ELF__
MAKE_ENV += "MAKEINFO=true"

include $(call select_from_repositories,mk/noux.mk)
