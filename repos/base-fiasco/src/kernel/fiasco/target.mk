TARGET = kernel-fiasco
LIBS   = kernel-fiasco

$(TARGET): $(INSTALL_DIR)/fiasco $(INSTALL_DIR)/sigma0-fiasco $(INSTALL_DIR)/bootstrap-fiasco

L4_BUILD_DIR = $(LIB_CACHE_DIR)/syscall-fiasco

$(INSTALL_DIR)/fiasco:
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/kernel-fiasco/build/fiasco $@

$(INSTALL_DIR)/sigma0-fiasco:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/bin/x86_586/l4v2/sigma0 $@

$(INSTALL_DIR)/bootstrap-fiasco:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/bin/x86_586/bootstrap $@

clean cleanall:
	$(VERBOSE)rm -f kernel sigma0 bootstrap
