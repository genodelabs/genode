TARGET   = intel_fb_drv
LIBS     = base intel_fb_drv intel_fb_include libc-setjmp server
SRC_CC   = main.cc lx_emul.cc dummies.cc
SRC_C    = i2c-algo-bit.c
INC_DIR += $(REP_DIR)/src/include
