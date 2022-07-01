REQUIRES := x86_64

TARGET  := test-driver_time
LIBS    := base pc_lx_emul jitterentropy

SRC_CC  += main.cc time.cc
SRC_C   += lx_user.c
SRC_C   += dummies.c
SRC_C   += generated_dummies.c

SRC_C   += lx_emul/common_dummies.c
SRC_C   += lx_emul/shadow/lib/kobject_uevent.c
SRC_C   += lx_emul/shadow/drivers/char/random.c
SRC_C   += lx_emul/shadow/kernel/softirq.c

vpath %.c  $(REP_DIR)/src/lib/pc
vpath %.cc $(REP_DIR)/src/lib/pc

LX_SRC_DIR := $(call select_from_ports,linux)/src/linux
ifeq ($(wildcard $(LX_SRC_DIR)),)
LX_SRC_DIR := $(call select_from_repositories,src/linux)
endif
ifeq ($(wildcard $(LX_SRC_DIR)),)
fail
endif

INC_DIR += $(PRG_DIR)
INC_DIR += $(LX_SRC_DIR)/drivers/gpu/drm/i915
