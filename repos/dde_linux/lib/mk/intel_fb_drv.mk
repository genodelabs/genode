LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/framebuffer/intel
SRC_DIR        := $(REP_DIR)/src/drivers/framebuffer/intel

LIBS    += intel_fb_include

SRC_C   :=
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/char/agp/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/i2c/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/i915/*.c))

#SRC_C := $(filter-out intel_dp.c,$(SRC_C))
#SRC_C := intel_panel.c

#
# Reduce build noise of compiling contrib code
#
CC_WARN = -Wall -Wno-uninitialized -Wno-unused-but-set-variable \
          -Wno-unused-variable -Wno-unused-function

vpath %.c $(LX_CONTRIB_DIR)/drivers/char/agp
vpath %.c $(LX_CONTRIB_DIR)/drivers/i2c
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm/i915
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm

