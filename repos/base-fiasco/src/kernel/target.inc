TARGET           = fiasco
REQUIRES        += fiasco
FIASCO_BUILD_DIR = $(BUILD_BASE_DIR)/kernel/$(TARGET)
FIASCO           = $(FIASCO_BUILD_DIR)/fiasco
FIASCO_SRC       = $(L4_SRC_DIR)/kernel/fiasco

$(TARGET): $(FIASCO)

$(FIASCO_BUILD_DIR):
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
                                     $(VERBOSE_DIR) -C $(FIASCO_SRC) BUILDDIR=$@
	$(VERBOSE)cp $(KERNEL_CONFIG) $@/globalconfig.out
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
                                     $(VERBOSE_DIR) -C $@ oldconfig

$(FIASCO): $(FIASCO_BUILD_DIR)
	$(VERBOSE_MK) MAKEFLAGS= $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
                                     $(VERBOSE_DIR) -C $(FIASCO_BUILD_DIR)
	$(VERBOSE)ln -sf $@ $(BUILD_BASE_DIR)/bin/$(TARGET)

clean cleanall:
	$(VERBOSE)rm -rf $(FIASCO_BUILD_DIR)
