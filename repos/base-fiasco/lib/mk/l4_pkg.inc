#
# Utility for building L4 contrib packages
#
# Variables that steer the behaviour of this makefile:
#
# TARGET - name of target
# PKGS   - list of L4 packages to visit in order to create
#          the target
#

ifeq ($(filter-out $(SPECS),x86_32),)
	L4_BUILD_ARCH := x86
endif

ifeq ($(filter-out $(SPECS),arm),)
	L4_BUILD_ARCH := arm
endif

ifeq ($(L4_BUILD_ARCH),)
$(error L4_BUILD_ARCH undefined, architecture not supported)
endif

ifndef L4_BUILD_DIR
$(error L4_BUILD_DIR undefined)
endif

L4_PKG_DIR = $(L4_SRC_DIR)/l4/pkg
PKG_TAGS   = $(addsuffix .tag,$(PKGS))

BUILD_OUTPUT_FILTER = 2>&1 | sed "s~^~      [$*]  ~"

ifeq ($(VERBOSE),)
L4_VERBOSE = V=1
endif

# do not confuse third-party sub-makes
unexport .SHELLFLAGS

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)
all: $(PKG_TAGS)
endif

#
# We preserve the order of processing 'l4/pkg/' directories because of
# inter-package dependencies. However, within each directory, make is working
# in parallel.
#
.NOTPARALLEL: $(PKG_TAGS)

WARN    = -Wno-attributes -Wno-cast-function-type -Wno-format-truncation \
          -Wno-frame-address -Wno-ignored-qualifiers -Wno-implicit-fallthrough \
          -Wno-maybe-uninitialized -Wno-misleading-indentation \
          -Wno-nonnull-compare -Wno-nonnull-compare -Wno-restrict \
          -Wno-tautological-compare -Wno-unused-but-set-variable -Wno-unused-result
CWARN   = $(WARN) -Wno-int-conversion -Wno-pointer-sign -Wno-pointer-to-int-cast
CXXWARN = $(WARN) -Wno-bool-compare -Wno-c++11-compat -Wno-class-memaccess

#
# The '_GNU_SOURCE' definition is needed to convince uClibc to define the
# 'off64_t' type, which is used by bootstrap.
#
%.tag:
	$(VERBOSE_MK) MAKEFLAGS= CPPFLAGS="$(CC_MARCH)" \
	              CFLAGS="$(CC_MARCH) -std=gnu89 $(CWARN) \
	                      -fno-tree-loop-distribute-patterns" \
	              CXXFLAGS="$(CC_MARCH) -D_GNU_SOURCE -std=gnu++98 $(CXXWARN) \
	                        -fno-tree-loop-distribute-patterns" \
	              ASFLAGS="$(CC_MARCH)" LDFLAGS="$(LD_MARCH)" \
	              $(MAKE) $(VERBOSE_DIR) O=$(L4_BUILD_DIR) $(L4_VERBOSE) \
	                      -C $(L4_PKG_DIR)/$* \
	                      CC="$(CROSS_DEV_PREFIX)gcc" \
	                      CXX="$(CROSS_DEV_PREFIX)g++" \
	                      LD="$(CROSS_DEV_PREFIX)ld" \
	                      $(BUILD_OUTPUT_FILTER)
	$(VERBOSE)mkdir -p $(dir $@) && touch $@

