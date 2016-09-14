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
	L4_BUILD_ARCH := x86
endif

ifeq ($(filter-out $(SPECS),arm),)
	L4_BUILD_ARCH := arm
endif

ifeq ($(L4_BUILD_ARCH),)
all: l4_build_arch_undefined
	$(VERBOSE)$(ECHO) "Error: L4_BUILD_ARCH undefined, architecture not supported"
	$(VERBOSE)false
endif

L4_PKG_DIR   = $(L4_SRC_DIR)/l4/pkg
PKG_TAGS     = $(addsuffix .tag,$(PKGS))

ifeq ($(VERBOSE),)
L4_VERBOSE = V=1
endif

$(TARGET): $(PKG_TAGS)

#
# We preserve the order of processing 'l4/pkg/' directories because of
# inter-package dependencies. However, within each directory, make is working
# in parallel.
#
.NOTPARALLEL: $(PKG_TAGS)

#
# The '_GNU_SOURCE' definition is needed to convince uClibc to define the
# 'off64_t' type, which is used by bootstrap.
#
%.tag:
	$(VERBOSE_MK) MAKEFLAGS= CPPFLAGS="$(CC_MARCH)" \
	              CFLAGS="$(CC_MARCH)" CXXFLAGS="$(CC_MARCH) -D_GNU_SOURCE" \
	              ASFLAGS="$(CC_MARCH)" LDFLAGS="$(LD_MARCH)" \
	              $(MAKE) $(VERBOSE_DIR) O=$(L4_BUILD_DIR) $(L4_VERBOSE) \
	                      -C $(L4_PKG_DIR)/$* \
	                      CC="$(CROSS_DEV_PREFIX)gcc" \
	                      CXX="$(CROSS_DEV_PREFIX)g++" \
	                      LD="$(CROSS_DEV_PREFIX)ld"
	$(VERBOSE)mkdir -p $(dir $@) && touch $@

clean cleanall: clean_tags

# if (pseudo) target is named after a directory, remove the whole subtree
clean_prg_objects: clean_dir_named_as_target

clean_dir_named_as_target:
	$(VERBOSE)(test -d $(TARGET) && rm -rf $(TARGET)) || true

clean_tags:
	$(VERBOSE)rm -f $(PKG_TAGS)

