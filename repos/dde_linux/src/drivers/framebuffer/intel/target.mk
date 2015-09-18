REQUIRES = x86

TARGET   = intel_fb_drv
LIBS     = base intel_fb_drv intel_fb_include libc-setjmp server config blit
SRC_CC   = main.cc lx_emul.cc dummies.cc
INC_DIR += $(REP_DIR)/src/include
