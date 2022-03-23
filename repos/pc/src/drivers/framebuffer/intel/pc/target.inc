REQUIRES := x86

REL_PRG_DIR := $(PRG_DIR)/../..

TARGET  := pc_intel_fb_drv
LIBS    := base pc_lx_emul blit

INC_DIR += $(REL_PRG_DIR)
INC_DIR += $(REL_PRG_DIR)/shadow

SRC_CC  += main.cc
SRC_CC  += misc.cc
SRC_CC  += time.cc
SRC_CC  += emul.cc
SRC_C   += dummies.c
SRC_C   += lx_emul.c
SRC_C   += $(notdir $(wildcard $(REL_PRG_DIR)/generated_dummies.c))
SRC_C   += fb.c
SRC_C   += lx_user.c
SRC_C   += gem.c
SRC_C   += common_dummies.c
SRC_C   += lx_emul/spec/x86/pci.c
SRC_C   += lx_emul/shadow/mm/page_alloc.c

vpath %.c $(REL_PRG_DIR)
vpath %.cc $(REL_PRG_DIR)
vpath common_dummies.c $(REP_DIR)/src/lib/pc/lx_emul

LX_SRC_DIR := $(call select_from_ports,linux)/src/linux
ifeq ($(wildcard $(LX_SRC_DIR)),)
LX_SRC_DIR := $(call select_from_repositories,src/linux)
endif
ifeq ($(wildcard $(LX_SRC_DIR)),)
fail
endif

INC_DIR    += $(LX_SRC_DIR)/drivers/gpu/drm/i915

CC_C_OPT += -Wno-unused-label

#
# Genode C-API backends
#
