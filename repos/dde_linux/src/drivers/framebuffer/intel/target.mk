REQUIRES = x86

TARGET   = intel_fb_drv
LIBS     = base intel_fb_drv intel_fb_include lx_kit_setjmp blit
SRC_CC   = main.cc lx_emul.cc
SRC_C    = dummies.c i915_params.c lx_emul_c.c

# lx_kit
SRC_CC  += irq.cc \
           malloc.cc \
           mapped_io_mem_range.cc \
           pci.cc \
           printf.cc \
           scheduler.cc \
           timer.cc \
           work.cc \
           env.cc

INC_DIR += $(REP_DIR)/src/include

vpath %.cc $(PRG_DIR)
vpath %.cc $(REP_DIR)/src/lx_kit

CC_CXX_WARN_STRICT =
CC_OPT += -Wno-narrowing
