REQUIRES = x86
TARGET   = pci_audio_drv
SRC_CC   = main.cc
LIBS     = dde_bsd_audio dde_bsd_audio_pci base
INC_DIR += $(REP_DIR)/include

vpath %.cc $(REP_DIR)/src/drivers/audio

CC_CXX_WARN_STRICT =
