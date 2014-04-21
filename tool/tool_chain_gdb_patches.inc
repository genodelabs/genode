GDB_PATCHES_DIR = $(GENODE_DIR)/tool/patches/gdb-$(GDB_VERSION)
GDB_PATCHES = $(shell cat $(GDB_PATCHES_DIR)/series)

$(CONTRIB_DIR)/gdb-$(GDB_VERSION)/configure:: $(CONTRIB_DIR)/gdb-$(GDB_VERSION)
	$(ECHO) "$(BRIGHT_COL)patching gdb...$(DEFAULT_COL)"
	$(VERBOSE)for p in $(GDB_PATCHES); do \
	            patch -d $(CONTRIB_DIR)/gdb-$(GDB_VERSION) -p1 -i $(GDB_PATCHES_DIR)/$$p; done;
	$(VERBOSE)touch $@
