#
# Create prerequisites for building Genode for Fiasco
#
# Prior building Genode programs for Fiasco, the kernel bindings must be
# generated. This is done by building a minimalistic subset of the original
# userland that comes with Fiasco.
#

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., the 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)

#
# Sanity checks
#
ifeq ($(L4_BUILD_DIR),$(BUILD_BASE_DIR)/l4)
all: $(L4_SRC_DIR)

$(L4_SRC_DIR):
	$(VERBOSE)$(ECHO) "--> Please, execute 'make prepare' in $(REP_DIR)"
	$(VERBOSE)$(ECHO) "--> before compiling Genode apps for Fiasco."
	$(VERBOSE)exit 1
endif

#
# Create L4 build directory
#
$(BUILD_BASE_DIR)/l4/.Makeconf.bid.old:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp $(L4_CONFIG) $(@:.old=)
	$(VERBOSE_MK) MAKEFLAGS= make $(VERBOSE_DIR) -C $(L4_SRC_DIR)/l4 \
         O=$(BUILD_BASE_DIR)/l4 SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" oldconfig

$(BUILD_BASE_DIR)/l4/pkg/uclibc/lib/uclibc:
	$(VERBOSE)mkdir -p $(BUILD_BASE_DIR)/l4/pkg/uclibc/lib/uclibc
	$(VERBOSE)tar cf - --exclude .svn -C $(L4_SRC_DIR)/../uclibc ARCH-all ARCH-x86 \
              | tar xf - -C $(BUILD_BASE_DIR)/l4/pkg/uclibc/lib/uclibc

PKGS = input/include \
       drivers/uart/include \
       l4sys/include \
       l4util/include \
       libc_support/include \
       libsigma0/include

include $(REP_DIR)/mk/l4_pkg.mk

all: $(PKG_TAGS)

$(PKG_TAGS): $(BUILD_BASE_DIR)/l4/.Makeconf.bid.old
$(PKG_TAGS): $(BUILD_BASE_DIR)/l4/pkg/uclibc/lib/uclibc

endif

