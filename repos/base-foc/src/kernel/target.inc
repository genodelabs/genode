TARGET        = fiasco.oc
REQUIRES     += foc
FOC_BUILD_DIR = $(BUILD_BASE_DIR)/kernel/$(TARGET)
FIASCO        = $(FOC_BUILD_DIR)/fiasco
FIASCO_SRC   := $(call select_from_ports,foc)/src/kernel/foc/kernel/fiasco

$(TARGET): $(FIASCO)

$(FOC_BUILD_DIR):
	$(VERBOSE_MK) $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
                          $(VERBOSE_DIR) -C $(FIASCO_SRC) BUILDDIR=$@
	$(VERBOSE)cp $(KERNEL_CONFIG) $@/globalconfig.out

$(FIASCO): $(FOC_BUILD_DIR)
	$(VERBOSE_MK) $(MAKE) SYSTEM_TARGET="$(CROSS_DEV_PREFIX)" \
                          CC="$(CC)" CXX="$(CXX)" \
                          $(VERBOSE_DIR) -C $(FOC_BUILD_DIR)
	$(VERBOSE)ln -sf $@ $(BUILD_BASE_DIR)/bin/$(TARGET)

clean cleanall:
	$(VERBOSE)rm -rf $(FOC_BUILD_DIR)
