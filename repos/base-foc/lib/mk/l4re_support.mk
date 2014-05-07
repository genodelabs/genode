#
# Build L4re base libraries, needed by sigma0 and bootstrap


# ignore stage one, visit the L4 build system at second build stage
ifeq ($(called_from_lib_mk),yes)

# packages in 'l4/pkg/'
PKGS = uclibc-headers uclibc-minimal l4util cxx

include $(REP_DIR)/mk/l4_pkg.mk
all: $(PKG_TAGS)

endif
