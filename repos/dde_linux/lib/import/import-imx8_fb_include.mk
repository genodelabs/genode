LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/framebuffer/imx8
SRC_DIR        := $(REP_DIR)/src/drivers/framebuffer/imx8

# architecture-dependent includes
ifeq ($(filter-out $(SPECS),arm_64),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_64 \
                      $(LX_CONTRIB_DIR)/arch/arm64/include \
                      $(LX_CONTRIB_DIR)/arch/arm64/include/uapi \
                      $(LX_CONTRIB_DIR)/arch/arm64/include/generated \
                      $(LX_CONTRIB_DIR)/arch/arm64/include/generated/uapi
endif # arm_64

INC_DIR += $(SRC_DIR)/include \
           $(REP_DIR)/src/include \
           $(ARCH_SRC_INC_DIR) \
           $(LX_CONTRIB_DIR)/drivers/gpu/drm \
           $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx \
           $(LX_CONTRIB_DIR)/include \
           $(LX_CONTRIB_DIR)/include/uapi \
           $(LIB_CACHE_DIR)/imx8_fb_include/include/include/include

CC_OPT += -U__linux__ -D__KERNEL__
