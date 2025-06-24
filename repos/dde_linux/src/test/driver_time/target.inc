TARGET  := test-driver_time
LIBS    := base virt_lx_emul jitterentropy

SRC_CC  += main.cc
SRC_C   += lx_user.c
SRC_C   += dummies.c
SRC_C   += generated_dummies.c
SRC_CC  += lx_emul/random.cc

vpath %.c  $(PRG_DIR)/../..
vpath %.cc $(PRG_DIR)/../..

LX_SRC_DIR := $(call select_from_ports,legacy_linux)/src/linux
ifeq ($(wildcard $(LX_SRC_DIR)),)
LX_SRC_DIR := $(call select_from_repositories,src/linux)
endif
ifeq ($(wildcard $(LX_SRC_DIR)),)
fail
endif

INC_DIR += $(PRG_DIR)/../..
INC_DIR += $(LX_SRC_DIR)/drivers/gpu/drm/i915
