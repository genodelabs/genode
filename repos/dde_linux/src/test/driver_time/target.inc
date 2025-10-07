TARGET  := test-driver_time
LIBS    := base virt_lx_emul jitterentropy

SRC_CC  += main.cc
SRC_C   += lx_user.c
SRC_C   += dummies.c
SRC_C   += generated_dummies.c
SRC_CC  += lx_emul/random.cc

vpath %.c  $(PRG_DIR)/../..
vpath %.cc $(PRG_DIR)/../..

INC_DIR += $(PRG_DIR)/../..
INC_DIR += $(LX_SRC_DIR)/drivers/gpu/drm/i915
