#
# Create prerequisites for building Genode for Fiasco.OC
#
# Prior building Genode programs for Fiasco.OC, the kernel bindings must be
# generated. This is done by building a minimalistic subset of the original
# userland (L4re) that comes with Fiasco.OC.
#

#
# We do this also in the first build stage to ensure that the kernel
# port, if missing, is added to the missing-ports list of this stage.
#
FOC_CONTRIB_DIR := $(call select_from_ports,foc)/src/kernel/foc

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
# Create L4 build directory
#
# Resetting the 'MAKEFLAGS' is important because otherwise, the L4
# build system will stuble over predefined variables, i.e., 'LIB'
#
$(BUILD_BASE_DIR)/l4/.kconfig:
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) $(VERBOSE_DIR) -C $(FOC_CONTRIB_DIR)/l4 B=$(dir $@) \
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

