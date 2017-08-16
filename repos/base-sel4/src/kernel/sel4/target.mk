TARGET = sel4
LIBS   = kernel-sel4

$(INSTALL_DIR)/$(TARGET):
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/kernel-sel4/kernel.elf.strip $@
