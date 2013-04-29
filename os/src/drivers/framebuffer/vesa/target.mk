TARGET    = fb_drv
REQUIRES  = vesa
SRC_CC    = main.cc framebuffer.cc ifx86emu.cc hw_emul.cc
CC_OPT   += -fomit-frame-pointer -DNO_SYS_HEADERS
SRC_C     = decode.c fpu.c ops.c ops2.c prim_ops.c sys.c
LIBS      = base blit
INC_DIR  += $(PRG_DIR)/include $(PRG_DIR)/contrib

vpath %.c $(PRG_DIR)/contrib

