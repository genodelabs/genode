TARGET = codezero

CODEZERO_DIR := $(call select_from_ports,codezero)/src/kernel/codezero

include $(REP_DIR)/lib/mk/codezero_cml.inc

TOOL_CHAIN_DIR     = $(dir $(CROSS_DEV_PREFIX))
CODEZERO_DST_DIR   = $(BUILD_BASE_DIR)/kernel/codezero
CODEZERO_BUILD_DIR = $(CODEZERO_DST_DIR)/build

.PHONY: $(TARGET)

MIRROR_COPY := conts/baremetal/empty conts/userlibs \
               build.py include SConstruct src loader

MIRROR_SYMLINK := scripts tools

update_copy = $(VERBOSE)tar c -C $(CODEZERO_DIR) $(MIRROR_COPY) | tar x -C $(CODEZERO_DST_DIR)

ifneq ($(VERBOSE),)
CODEZERO_STDOUT := > /dev/null
endif

#
# Environment variables passed to the Codezero build system
#
BUILD_ENV = PATH=$(dir $(CROSS_DEV_PREFIX)):$$PATH

#
# Local copy of the CML file used for supplying the configuration
# to the Codezero build system.
#
LOCAL_CONFIG_CML := $(shell pwd)/config.cml

$(TARGET): $(CODEZERO_BUILD_DIR)
	$(MSG_BUILD)kernel
	$(update_copy)
	$(VERBOSE)cd $(CODEZERO_DST_DIR); $(BUILD_ENV) ./build.py $(CODEZERO_STDOUT)

#
# Mirror the parts of the Codezero source tree that are relevant for building
# the kernel
#
$(CODEZERO_DST_DIR): $(CODEZERO_DIR)
	$(VERBOSE)test -d $@ || mkdir -p $@
	$(VERBOSE)for d in $(MIRROR_SYMLINK); do ln -sf $(realpath $^)/$$d $@/$$d; done

$(CODEZERO_BUILD_DIR): $(CODEZERO_DST_DIR) $(CODEZERO_CML)
	$(update_copy)
	$(VERBOSE)cp $(CODEZERO_CML) $(LOCAL_CONFIG_CML)
	@#
	@# Create copy of the CML config in the local build directory to update
	@# the tool chain parameters according to the CROSS_DEV_PREFIX configured
	@# for Genode.
	@#
	$(VERBOSE)sed -i "/TOOLCHAIN_USERSPACE/s/\".*\"/\"$(notdir $(CROSS_DEV_PREFIX))\"/" $(LOCAL_CONFIG_CML)
	$(VERBOSE)sed -i "/TOOLCHAIN_KERNEL/s/\".*\"/\"$(notdir $(CROSS_DEV_PREFIX))\"/" $(LOCAL_CONFIG_CML)
	$(VERBOSE)cd $(CODEZERO_DST_DIR); $(BUILD_ENV) ./build.py -C -b -f $(LOCAL_CONFIG_CML) $(CODEZERO_STDOUT)

clean cleanall: clean_codezero

#
# Make sure to execute the 'clean_codezero' rule prior the generic clean
# rule in 'prg.mk' because the generic rule will attempt to remove $(TARGET)
# file, which is a directory in our case.
#
clean_prg_objects: clean_codezero

clean_codezero:
	$(VERBOSE)rm -f  $(LOCAL_CONFIG_CML)
	$(VERBOSE)rm -rf $(CODEZERO_DST_DIR)
