include $(REP_DIR)/lib/import/import-av.inc

TARGET = avplay
SRC_C = avplay.c cmdutils.c libc_dummies.c
LIBS += avfilter avformat swscale sdl libc libc_log libc_rom config_args

CC_WARN += -Wno-parentheses -Wno-switch -Wno-uninitialized -Wno-format-zero-length -Wno-pointer-sign

# config.h
INC_DIR += $(REP_DIR)/src/lib/libav

CC_C_OPT += -DLIBAV_VERSION=\"0.8\"

vpath %.c $(REP_DIR)/contrib/$(LIBAV)
