GCC_PATCHES_DIR = $(GENODE_DIR)/tool/patches/gcc-$(GCC_VERSION)
GCC_PATCHES = $(shell cat $(GCC_PATCHES_DIR)/series)

$(CONTRIB_DIR)/gcc-$(GCC_VERSION)/configure:: $(CONTRIB_DIR)/gcc-$(GCC_VERSION)
	$(ECHO) "$(BRIGHT_COL)patching gcc build system...$(DEFAULT_COL)"
	$(VERBOSE)for p in $(GCC_PATCHES); do \
	            patch -d $(CONTRIB_DIR)/gcc-$(GCC_VERSION) -p1 -i $(GCC_PATCHES_DIR)/$$p; done;
	@#
	@# Re-generate configure scripts
	@#
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION); autogen Makefile.def
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libgcc; $(AUTOCONF)
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/libstdc++-v3; $(AUTOCONF)
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION)/gcc; $(AUTOCONF)
	$(VERBOSE)cd $(CONTRIB_DIR)/gcc-$(GCC_VERSION); $(AUTOCONF)
	$(VERBOSE)touch $@
