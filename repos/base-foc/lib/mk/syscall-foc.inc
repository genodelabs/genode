#
# Create prerequisites for building Genode for Fiasco.OC
#
# Prior building Genode programs for Fiasco.OC, the kernel bindings must be
# generated. This is done by building a minimalistic subset of the original
# userland (L4re) that comes with Fiasco.OC.
#

L4_SRC_DIR   := $(call select_from_ports,foc)/src/kernel/foc
L4_BUILD_DIR := $(shell pwd)/build

#
# Create mirror for architecture-specific L4sys header files
#
L4_INC_TARGETS += l4/sys \
                  l4f/l4/sys \
                  l4/sigma0 \
                  l4/vcpu

CC_OPT += -Iinclude

include/%:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/include/$* $@

#
# Use 'regparm=0' call instead of an inline function, when accessing
# the utcb. This is needed to stay compatible with L4linux
#
CC_OPT += -DL4SYS_USE_UTCB_WRAP=1

#
# Create L4 build directory
#
# Resetting the 'MAKEFLAGS' is important because otherwise, the L4
# build system will stuble over predefined variables, i.e., 'LIB'
#
$(L4_BUILD_DIR)/.kconfig:
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) $(VERBOSE_DIR) -C $(L4_SRC_DIR)/l4 \
	     B=$(L4_BUILD_DIR) DROPSCONF_DEFCONFIG="$(L4_CONFIG)" \
	     VERBOSE="$(VERBOSE)" SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
	     2>&1 | sed "s/^/      [l4build]  /"

PKGS := ldscripts \
        libgcc-pure \
        l4sys \
        libgcc \
        libsigma0 \
        libvcpu/include

include $(REP_DIR)/lib/mk/l4_pkg.inc

$(PKG_TAGS): $(L4_BUILD_DIR)/.kconfig
$(PKG_TAGS): $(addprefix include/,$(L4_INC_TARGETS))

#
# Compile 'syscall-foc.lib.a' not before the 'PKGS' are completely built
#
$(SRC_S) $(SRC_C) : $(PKG_TAGS)
