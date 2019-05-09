BOARD ?= unknown
TARGET = kernel-foc
LIBS   = kernel-foc

$(TARGET): $(INSTALL_DIR)/foc-$(BOARD)

$(INSTALL_DIR)/foc-$(BOARD): $(LIB_CACHE_DIR)/kernel-foc/$(BOARD)-build/fiasco
	$(VERBOSE)ln -sf $< $@
