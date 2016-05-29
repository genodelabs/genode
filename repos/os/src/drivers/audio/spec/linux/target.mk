REQUIRES = linux
TARGET   = audio_drv
LIBS     = lx_hybrid
SRC_CC   = main.cc
SRC_C    = alsa.c
INC_DIR += $(PRG_DIR)
LX_LIBS  = alsa
