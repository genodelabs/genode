L4_SRC_DIR = $(call select_from_ports,fiasco)/src/kernel/fiasco/fiasco/snapshot

FIASCO_BUILD_DIR = $(shell pwd)/build
FIASCO           = $(FIASCO_BUILD_DIR)/fiasco
FIASCO_SRC       = $(L4_SRC_DIR)/kernel/fiasco

KERNEL_BUILD_OUTPUT_FILTER = 2>&1 | sed "s/^/      [fiasco]  /"

$(FIASCO_BUILD_DIR):
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
	                                 $(VERBOSE_DIR) -C $(FIASCO_SRC) BUILDDIR=$@ \
	                                 $(KERNEL_BUILD_OUTPUT_FILTER)
	$(VERBOSE)cp $(KERNEL_CONFIG) $@/globalconfig.out
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
	                                 $(VERBOSE_DIR) -C $@ oldconfig \
	                                 $(KERNEL_BUILD_OUTPUT_FILTER)
	$(VERBOSE)cp $(KERNEL_CONFIG) $@/globalconfig.out

$(FIASCO): $(FIASCO_BUILD_DIR)
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
	                                 $(VERBOSE_DIR) -C $(FIASCO_BUILD_DIR) \
	                                 $(KERNEL_BUILD_OUTPUT_FILTER)

#
# Sigma0 and bootstrap are provided as L4 packages
#
# We build them within the L4 build directory created by the 'syscall-fiasco'
# library.
#

LIBS += syscall-fiasco

PKGS = l4sys/lib \
       uclibc/lib/uclibc \
       uclibc/lib/include \
       crtx \
       l4util/lib \
       cxx \
       sigma0/server \
       bootstrap

L4_BUILD_DIR := $(LIB_CACHE_DIR)/syscall-fiasco

include $(REP_DIR)/lib/mk/l4_pkg.inc

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)
all: $(FIASCO)
endif

