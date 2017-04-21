#
# Create prerequisites for building Genode for Fiasco
#
# Prior building Genode programs for Fiasco, the kernel bindings must be
# generated. This is done by building a minimalistic subset of the original
# userland that comes with Fiasco.
#

L4_SRC_DIR    = $(call select_from_ports,fiasco)/src/kernel/fiasco/fiasco/snapshot
L4_BUILD_DIR := $(shell pwd)

#
# Create L4 build directory
#
.Makeconf.bid.old:
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)cp $(L4_CONFIG) $(@:.old=)
	$(VERBOSE_MK) set -o pipefail; \
	   MAKEFLAGS= make $(VERBOSE_DIR) -C $(L4_SRC_DIR)/l4 \
	   O=$(L4_BUILD_DIR) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" oldconfig \
	   2>&1 | sed "s/^/      [l4build]  /"
	$(VERBOSE)touch $@

pkg/uclibc/lib/uclibc:
	$(VERBOSE)mkdir -p pkg/uclibc/lib/uclibc
	$(VERBOSE)tar cf - --exclude .svn -C $(L4_SRC_DIR)/../uclibc ARCH-all ARCH-x86 \
	        | tar xf - -C pkg/uclibc/lib/uclibc

PKGS = input/include \
       drivers/uart/include \
       l4sys/include \
       l4util/include \
       libc_support/include \
       libsigma0/include

include $(REP_DIR)/lib/mk/l4_pkg.inc

$(PKG_TAGS): .Makeconf.bid.old
$(PKG_TAGS): pkg/uclibc/lib/uclibc
