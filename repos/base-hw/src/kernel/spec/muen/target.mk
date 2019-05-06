TARGET          = muen
REQUIRES        = x86_64 muen
MUEN_SRC_DIR    = $(call select_from_ports,muen)/src/kernel/muen
MUEN_BUILD_DIR  = $(BUILD_BASE_DIR)/kernel
MUEN_CONF_FILE  = $(MUEN_BUILD_DIR)/muen.conf
MUEN_DST_DIR    = $(MUEN_BUILD_DIR)/muen
MUEN_LOG        = $(MUEN_BUILD_DIR)/build.log

MUEN_SYSTEM     = $(shell sed -n "/^SYSTEM/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
MUEN_HARDWARE   = $(shell sed -n "/^HARDWARE/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
MUEN_PLATFORM   = $(shell sed -n "/^PLATFORM/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
MUEN_COMPONENTS = $(shell sed -n "/^COMPONENTS/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
GNAT_PATH       = $(shell sed -n "/^GNAT_PATH/s/^.*=\\s*//p" ${MUEN_CONF_FILE})
SPARK_PATH      = $(shell sed -n "/^SPARK_PATH/s/^.*=\\s*//p" ${MUEN_CONF_FILE})

BUILD_ENV       = PATH=$(GNAT_PATH)/bin:$(SPARK_PATH)/bin:$$PATH
BUILD_OPTS      = SYSTEM=$(MUEN_SYSTEM) HARDWARE=$(MUEN_HARDWARE) PLATFORM=$(MUEN_PLATFORM) NO_PROOF=true

ifeq ($(VERBOSE),)
BUILD_OPTS     += BUILD_OUTPUT_VERBOSE=true
else
BUILD_OPTS     += BUILD_OUTPUT_NOCOLOR=true
endif


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

$(MUEN_DST_DIR): $(MUEN_SRC_DIR)
	$(VERBOSE)mkdir -p $(MUEN_DST_DIR)
	$(VERBOSE)tar c -C $(MUEN_SRC_DIR) . | tar x -m -C $(MUEN_DST_DIR)

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
