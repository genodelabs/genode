GCC_VERSION  = 4.6.1
GCC          = gcc-$(GCC_VERSION)
GCC_URL      = ftp://ftp.fu-berlin.de/gnu/gcc

GCC_CORE_TGZ = gcc-core-$(GCC_VERSION).tar.gz
GCC_CXX_TGZ  = gcc-g++-$(GCC_VERSION).tar.gz

#
# Interface to top-level prepare Makefile
#
PORTS += $(GCC)

prepare:: $(CONTRIB_DIR)/$(GCC)/configure

#
# Port-specific local rules
#

$(DOWNLOAD_DIR)/$(GCC_CORE_TGZ):
	$(VERBOSE)wget -P $(DOWNLOAD_DIR) $(GCC_URL)/$(GCC)/$(GCC_CORE_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(GCC_CXX_TGZ):
	$(VERBOSE)wget -P $(DOWNLOAD_DIR) $(GCC_URL)/$(GCC)/$(GCC_CXX_TGZ) && touch $@

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

$(CONTRIB_DIR)/$(GCC): $(DOWNLOAD_DIR)/$(GCC_CORE_TGZ) $(DOWNLOAD_DIR)/$(GCC_CXX_TGZ)
	$(VERBOSE)for i in $^ ; do tar xfz $$i -C $(CONTRIB_DIR) ;done

include ../tool/tool_chain_gcc_patches.inc

$(CONTRIB_DIR)/$(GCC)/configure:: $(CONTRIB_DIR)/$(GCC)
	@#
	@# Noux-specific changes
	@#
	$(VERBOSE)patch -d $(CONTRIB_DIR)/$(GCC) -N -p1 < src/noux-pkg/gcc/build.patch
