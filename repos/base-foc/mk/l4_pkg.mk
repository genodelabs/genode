#
# Utility for building L4 contrib packages
#
# Variables that steer the behaviour of this makefile:
#
# TARGET - name of target
# PKGS   - list of L4 packages to visit in order to create
#          the target
#

LIBS += platform

ifeq ($(filter-out $(SPECS),x86_32),)
	L4_BUILD_ARCH := x86_586
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	L4_BUILD_ARCH := amd_k9
endif

ifeq ($(filter-out $(SPECS),arm_v7a),)
	L4_BUILD_ARCH := arm_armv7a
endif

ifeq ($(filter-out $(SPECS),arm_v6),)
	L4_BUILD_ARCH := arm_armv6
endif

ifeq ($(L4_BUILD_ARCH),)
all: l4_build_arch_undefined
	$(VERBOSE)$(ECHO) "Error: L4_BUILD_ARCH undefined, architecture not supported"
	$(VERBOSE)false
endif

L4_BUILD_DIR = $(BUILD_BASE_DIR)/l4
L4_BUILD_OPT = SYSTEM_TARGET=$(CROSS_DEV_PREFIX)
L4_PKG_DIR  := $(call select_from_ports,foc)/src/kernel/foc/l4/pkg
PKG_TAGS     = $(addsuffix .tag,$(PKGS))

$(TARGET): $(PKG_TAGS)

#
# We preserve the order of processing 'l4/pkg/' directories because of
# inter-package dependencies. However, within each directory, make is working
# in parallel.
#
.NOTPARALLEL: $(PKG_TAGS)

%.tag:
	$(VERBOSE_MK) $(MAKE) $(VERBOSE_DIR) O=$(L4_BUILD_DIR) -C $(L4_PKG_DIR)/$* "$(L4_BUILD_OPT)"
	$(VERBOSE)mkdir -p $(dir $@) && touch $@

clean cleanall: clean_tags

# if (pseudo) target is named after a directory, remove the whole subtree
clean_prg_objects: clean_dir_named_as_target

clean_dir_named_as_target:
	$(VERBOSE)(test -d $(TARGET) && rm -rf $(TARGET)) || true

clean_tags:
	$(VERBOSE)rm -f $(PKG_TAGS)

