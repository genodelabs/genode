#
# Create prerequisites for building Genode for Fiasco.OC
#
# Prior building Genode programs for Fiasco.OC, the kernel bindings must be
# generated. This is done by building a minimalistic subset of the original
# userland (L4re) that comes with Fiasco.OC.
#

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., the 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)

#
# Create mirror for architecture-specific L4sys header files
#
L4_INC_TARGETS += l4/sys \
                  l4f/l4/sys \
                  l4/sigma0 \
                  l4/vcpu

all: $(addprefix $(BUILD_BASE_DIR)/include/,$(L4_INC_TARGETS))

$(BUILD_BASE_DIR)/include/%:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/include/$* $@

#
# Sanity checks
#
ifeq ($(L4_BUILD_DIR),$(BUILD_BASE_DIR)/l4)
ifeq ($(L4_CONFIG),)
all: $(REP_DIR)/contrib l4_config_not_defined
l4_config_not_defined:
	$(VERBOSE)$(ECHO) "Error: L4_CONFIG is not defined, platform not supported"
	@false
endif
endif

$(REP_DIR)/contrib:
	$(VERBOSE)$(ECHO) "--> Please, execute 'make prepare' in $(REP_DIR)"
	$(VERBOSE)$(ECHO) "--> before compiling Genode apps for Fiasco.OC."
	$(VERBOSE)$(ECHO) "--> Run 'make cleanall' before next compilation."
	$(VERBOSE)exit 1

#
# Create L4 build directory
#
# Resetting the 'MAKEFLAGS' is important because otherwise, the L4
# build system will stuble over predefined variables, i.e., 'LIB'
#
$(BUILD_BASE_DIR)/l4/.kconfig: $(REP_DIR)/contrib
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) $(VERBOSE_DIR) -C $(REP_DIR)/contrib/l4 B=$(dir $@) \
	     DROPSCONF_DEFCONFIG="$(L4_CONFIG)" \
	     VERBOSE="$(VERBOSE)" SYSTEM_TARGET="$(CROSS_DEV_PREFIX)"

PKGS = ldscripts \
       libgcc-pure \
       l4sys \
       libgcc \
       libsigma0 \
       libvcpu/include

include $(REP_DIR)/mk/l4_pkg.mk
all: $(PKG_TAGS)

$(PKG_TAGS): $(BUILD_BASE_DIR)/l4/.kconfig

endif

