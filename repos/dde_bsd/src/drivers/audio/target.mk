REQUIRES = x86 pci
TARGET   = audio_drv
SRC_CC   = main.cc
LIBS     = dde_bsd_audio dde_bsd_audio_pci base
INC_DIR += $(REP_DIR)/include

CC_CXX_WARN_STRICT =
