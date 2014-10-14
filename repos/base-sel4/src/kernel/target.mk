TARGET = sel4
LIBS   = kernel

$(INSTALL_DIR)/$(TARGET):
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/kernel/kernel.elf $@
