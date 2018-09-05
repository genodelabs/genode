LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/framebuffer/intel
SRC_DIR        := $(REP_DIR)/src/drivers/framebuffer/intel

# architecture-dependent includes
ifeq ($(filter-out $(SPECS),x86),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86 \
                      $(LX_CONTRIB_DIR)/arch/x86/include
  ifeq ($(filter-out $(SPECS),32bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_32
  endif # 32bit
  ifeq ($(filter-out $(SPECS),64bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_64
  endif # 64bit
endif # x86

INC_DIR += $(SRC_DIR)/include \
           $(REP_DIR)/src/include \
           $(ARCH_SRC_INC_DIR) \
           $(LX_CONTRIB_DIR)/drivers/gpu/drm \
           $(LX_CONTRIB_DIR)/drivers/gpu/include/drm \
           $(LX_CONTRIB_DIR)/drivers/gpu/include \
           $(LX_CONTRIB_DIR)/include \
           $(LX_CONTRIB_DIR)/include/uapi \
           $(LIB_CACHE_DIR)/intel_fb_include/include/include/include

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_DRM_I915_KMS -DCONFIG_I2C -DCONFIG_I2C_BOARDINFO
CC_OPT += -DCONFIG_ACPI
