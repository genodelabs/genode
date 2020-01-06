LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/framebuffer/imx8
SRC_DIR        := $(REP_DIR)/src/drivers/framebuffer/imx8

LIBS    += imx8_fb_include

SRC_C   :=
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/dma-buf/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/i2c/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/base/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx/dcss/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx/hdp/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/gpu/imx/dcss/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/irqchip/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/mxc/hdp/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/drivers/video/*.c))
SRC_C   += $(notdir $(wildcard $(LX_CONTRIB_DIR)/lib/*.c))

#
# Linux sources are C89 with GNU extensions
#
CC_C_OPT += -std=gnu89

#
# Reduce build noise of compiling contrib code
#
CC_WARN = -Wall -Wno-uninitialized -Wno-unused-but-set-variable \
          -Wno-unused-variable -Wno-unused-function \
          -Wno-pointer-arith -Wno-pointer-sign \
          -Wno-int-to-pointer-cast -Wno-stringop-truncation

vpath %.c $(LX_CONTRIB_DIR)/drivers/base
vpath %.c $(LX_CONTRIB_DIR)/drivers/dma-buf
vpath %.c $(LX_CONTRIB_DIR)/drivers/i2c
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx/dcss
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/drm/imx/hdp
vpath %.c $(LX_CONTRIB_DIR)/drivers/gpu/imx/dcss
vpath %.c $(LX_CONTRIB_DIR)/drivers/irqchip
vpath %.c $(LX_CONTRIB_DIR)/drivers/mxc/hdp
vpath %.c $(LX_CONTRIB_DIR)/drivers/video
vpath %.c $(LX_CONTRIB_DIR)/lib


CC_CXX_WARN_STRICT =
