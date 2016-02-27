BINUTILS_PATCHES_DIR = $(GENODE_DIR)/tool/patches/binutils-$(BINUTILS_VERSION)
BINUTILS_PATCHES = $(shell cat $(BINUTILS_PATCHES_DIR)/series)

$(CONTRIB_DIR)/binutils-$(BINUTILS_VERSION)/configure:: $(CONTRIB_DIR)/binutils-$(BINUTILS_VERSION)
	$(ECHO) "$(BRIGHT_COL)patching binutils build system...$(DEFAULT_COL)"
	$(VERBOSE)for p in $(BINUTILS_PATCHES); do \
	            patch -d $(CONTRIB_DIR)/binutils-$(BINUTILS_VERSION) -p1 -i $(BINUTILS_PATCHES_DIR)/$$p; done;
	@#
	@# Re-generate configure scripts
	@#
	$(VERBOSE)cd $(CONTRIB_DIR)/binutils-$(BINUTILS_VERSION)/bfd; $(AUTOCONF)
	$(VERBOSE)touch $@
