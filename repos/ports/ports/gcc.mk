GCC_VERSION = 4.7.2
GCC         = gcc-$(GCC_VERSION)
GCC_URL     = ftp://ftp.fu-berlin.de/gnu/gcc
GCC_TGZ     = gcc-$(GCC_VERSION).tar.gz
GCC_SIG     = $(GCC_TGZ).sig
GCC_KEY     = GNU

#
# Interface to top-level prepare Makefile
#
PORTS += $(GCC)

prepare-gcc: $(CONTRIB_DIR)/$(GCC)/configure

#
# Port-specific local rules
#

$(DOWNLOAD_DIR)/$(GCC_TGZ):
	$(VERBOSE)wget -P $(DOWNLOAD_DIR) $(GCC_URL)/$(GCC)/$(GCC_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(GCC_SIG):
	$(VERBOSE)wget -P $(DOWNLOAD_DIR) $(GCC_URL)/$(GCC)/$(GCC_SIG) && touch $@

$(DOWNLOAD_DIR)/$(GCC_TGZ).verified: $(DOWNLOAD_DIR)/$(GCC_TGZ) \
                                     $(DOWNLOAD_DIR)/$(GCC_SIG)
	$(VERBOSE)$(SIGVERIFIER) $(DOWNLOAD_DIR)/$(GCC_TGZ) $(DOWNLOAD_DIR)/$(GCC_SIG) $(GCC_KEY)
	$(VERBOSE)touch $@

#
# Utilities
#
AUTOCONF = autoconf2.64

#
# Check if 'autoconf' is installed
#
ifeq ($(shell which $(AUTOCONF)),)
$(error Need to have '$(AUTOCONF)' installed.)
endif

#
# Check if 'autogen' is installed
#
ifeq ($(shell which autogen)),)
$(error Need to have 'autogen' installed.)
endif

$(CONTRIB_DIR)/$(GCC): $(DOWNLOAD_DIR)/$(GCC_TGZ).verified
	$(VERBOSE)tar xfz $(<:.verified=) -C $(CONTRIB_DIR)

# needed in 'tool_chain_gcc_patches.inc'
GENODE_DIR := $(abspath ../..)

include $(GENODE_DIR)/tool/tool_chain_gcc_patches.inc

$(CONTRIB_DIR)/$(GCC)/configure:: $(CONTRIB_DIR)/$(GCC)
	@#
	@# Noux-specific changes
	@#
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(GCC) -N -p1 < src/noux-pkg/gcc/build.patch
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(GCC) -N -p1 < src/noux-pkg/gcc/build_with_makeinfo_5.patch

clean-gcc:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(GCC)
