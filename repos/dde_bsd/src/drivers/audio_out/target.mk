TARGET   = audio_out_drv
REQUIRES = x86
SRC_CC   = main.cc
LIBS     = dde_bsd_audio base config server
INC_DIR += $(REP_DIR)/include
