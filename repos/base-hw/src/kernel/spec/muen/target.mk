TARGET          = muen
REQUIRES        = x86_64 muen
MUEN_SRC_DIR    = $(call select_from_ports,muen)/src/kernel/muen
MUEN_BUILD_DIR  = $(BUILD_BASE_DIR)/kernel
MUEN_CONF_FILE  = $(MUEN_BUILD_DIR)/muen.conf
MUEN_DST_DIR    = $(MUEN_BUILD_DIR)/muen
MUEN_LOG        = $(MUEN_BUILD_DIR)/build.log

MUEN_SYSTEM     = $(shell sed -n "/^SYSTEM/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
MUEN_HARDWARE   = $(shell sed -n "/^HARDWARE/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
MUEN_COMPONENTS = $(shell sed -n "/^COMPONENTS/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
GNAT_PATH       = $(shell sed -n "/^GNAT_PATH/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
SPARK_PATH      = $(shell sed -n "/^SPARK_PATH/s/^.*=\\s*//p" ${MUEN_CONF_FILE})

BUILD_ENV       = PATH=$(GNAT_PATH)/bin:$(SPARK_PATH)/bin:$$PATH
BUILD_OPTS      = SYSTEM=$(MUEN_SYSTEM) HARDWARE=$(MUEN_HARDWARE) NO_PROOF=true

ifneq ($(filter muen, $(SPECS)),)
$(TARGET): $(MUEN_DST_DIR)
	$(MSG_BUILD)Muen policy
	$(VERBOSE)$(BUILD_ENV) $(MAKE) -C $(MUEN_DST_DIR) $(BUILD_OPTS) policy-merge rts >> $(MUEN_LOG) 2>&1
	$(MSG_BUILD)Muen components
	$(VERBOSE)$(BUILD_ENV) $(MAKE) -C $(MUEN_DST_DIR)/components \
		COMPONENTS=$(MUEN_COMPONENTS) $(BUILD_OPTS) >> $(MUEN_LOG) 2>&1
	$(MSG_BUILD)Muen kernel
	$(VERBOSE)$(BUILD_ENV) $(MAKE) -C $(MUEN_DST_DIR)/policy $(BUILD_OPTS) compile >> $(MUEN_LOG) 2>&1
	$(VERBOSE)$(BUILD_ENV) $(MAKE) -C $(MUEN_DST_DIR)/kernel $(BUILD_OPTS) >> $(MUEN_LOG) 2>&1
	$(VERBOSE)$(BUILD_ENV) $(MAKE) -C $(MUEN_DST_DIR)/components $(BUILD_OPTS) install-tau0 >> $(MUEN_LOG) 2>&1

$(MUEN_DST_DIR): download_contrib
	$(VERBOSE)mkdir -p $(MUEN_DST_DIR)
	$(VERBOSE)tar c -C $(MUEN_SRC_DIR) . | tar x -C $(MUEN_DST_DIR)

download_contrib: $(MUEN_SRC_DIR)
	$(MSG_BUILD)Muen contrib
	$(VERBOSE)cd $(MUEN_SRC_DIR) && git submodule update --init tools/mugenschedcfg > $(MUEN_LOG) 2>&1
	$(VERBOSE)cd $(MUEN_SRC_DIR) && git submodule update --init components/libxhcidbg > $(MUEN_LOG) 2>&1
	$(VERBOSE)$(BUILD_ENV) $(MAKE) -C $(MUEN_SRC_DIR)/contrib \
		QUIET=true download >> $(MUEN_LOG) 2>&1

clean cleanall: clean_muen

#
# Make sure to execute the 'clean_muen' rule prior the generic clean
# rule in 'prg.mk' because the generic rule will attempt to remove $(TARGET)
# file, which is a directory in our case.
#
clean_prg_objects: clean_muen

clean_muen:
	$(VERBOSE)rm -rf $(MUEN_DST_DIR)
	$(VERBOSE)rm -f  $(MUEN_CONF_FILE)
	$(VERBOSE)rm -f  $(MUEN_LOG)
endif


.PHONY: $(TARGET)
