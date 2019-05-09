BOARD ?= unknown
TARGET = sel4-$(BOARD)
LIBS   = kernel-sel4-$(BOARD)

$(INSTALL_DIR)/$(TARGET):
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/kernel-sel4-$(BOARD)/kernel.elf.strip $@
