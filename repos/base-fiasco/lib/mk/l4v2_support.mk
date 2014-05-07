#
# Build L4env base libraries, needed by sigma0 and bootstrap


# ignore stage one, visit the L4 build system at second build stage
ifeq ($(called_from_lib_mk),yes)

# packages in 'l4/pkg/'
PKGS = l4sys/lib \
       uclibc/lib/uclibc \
       uclibc/lib/include \
       crtx \
       l4util/lib \
       cxx

include $(REP_DIR)/mk/l4_pkg.mk
all: $(PKG_TAGS)

endif
