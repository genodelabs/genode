TARGET = kernel-foc
LIBS   = kernel-foc

$(TARGET): $(INSTALL_DIR)/foc

$(INSTALL_DIR)/foc: $(LIB_CACHE_DIR)/kernel-foc/build/fiasco
	$(VERBOSE)ln -sf $< $@
