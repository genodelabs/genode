GCC_VERSION  = 4.4.5
GCC          = gcc-$(GCC_VERSION)
GCC_URL      = ftp://ftp.fu-berlin.de/gnu/gcc

GCC_CORE_TGZ = gcc-core-$(GCC_VERSION).tar.gz
GCC_CXX_TGZ  = gcc-g++-$(GCC_VERSION).tar.gz

#
# Interface to top-level prepare Makefile
#
PORTS += $(GCC)

prepare:: $(CONTRIB_DIR)/$(GCC)

#
# Port-specific local rules
#

$(DOWNLOAD_DIR)/$(GCC_CORE_TGZ):
	$(VERBOSE)wget -P $(DOWNLOAD_DIR) $(GCC_URL)/$(GCC)/$(GCC_CORE_TGZ) && touch $@

$(DOWNLOAD_DIR)/$(GCC_CXX_TGZ):
	$(VERBOSE)wget -P $(DOWNLOAD_DIR) $(GCC_URL)/$(GCC)/$(GCC_CXX_TGZ) && touch $@

$(CONTRIB_DIR)/$(GCC): $(DOWNLOAD_DIR)/$(GCC_CORE_TGZ) $(DOWNLOAD_DIR)/$(GCC_CXX_TGZ)
	$(VERBOSE)for i in $^ ; do tar xfz $$i -C $(CONTRIB_DIR) ;done
	$(VERBOSE)patch -N -p0 < src/noux-pkg/gcc/build.patch

