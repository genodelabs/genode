REQUIRES := x86

REL_PRG_DIR := $(PRG_DIR)/../..

TARGET  := pc_intel_fb
LIBS    := base pc_lx_emul blit jitterentropy

INC_DIR += $(REL_PRG_DIR)

SRC_CC  += main.cc
SRC_CC  += emul.cc
SRC_CC  += opregion_io_mem.cc
SRC_CC  += lx_emul/shared_dma_buffer.cc
SRC_C   += dummies.c
SRC_C   += pci.c
SRC_C   += lx_emul.c
SRC_C   += $(notdir $(wildcard $(REL_PRG_DIR)/generated_dummies.c))
SRC_C   += fb.c
SRC_C   += lx_user.c
SRC_C   += gem.c
SRC_C   += lx_emul/common_dummies.c
SRC_C   += lx_emul/shadow/mm/page_alloc.c
SRC_C   += lx_emul/shadow/drivers/char/random.c

vpath %.c  $(REL_PRG_DIR)
vpath %.cc $(REL_PRG_DIR)

vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

LX_SRC_DIR := $(call select_from_ports,legacy_linux)/src/linux
ifeq ($(wildcard $(LX_SRC_DIR)),)
LX_SRC_DIR := $(call select_from_repositories,src/linux)
endif
ifeq ($(wildcard $(LX_SRC_DIR)),)
fail
endif

INC_DIR    += $(LX_SRC_DIR)/drivers/gpu/drm/i915
INC_DIR    += $(LX_SRC_DIR)/drivers/gpu/drm

#
# The generated dummies file pulls in functions without header
# includes so we force the inclusion of i915_drv explicitly for
# this compilation unit.
# 
CC_OPT_generated_dummies += -include $(LX_SRC_DIR)/drivers/gpu/drm/i915/i915_drv.h

CC_C_OPT += -Wno-unused-label

#
# Genode C-API backends
#
