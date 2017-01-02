TARGET    = fb_drv
REQUIRES  = x86
SRC_CC    = main.cc framebuffer.cc ifx86emu.cc hw_emul.cc
LIBS      = base blit x86emu
INC_DIR  += $(PRG_DIR)/include $(REP_DIR)/include/x86emu
