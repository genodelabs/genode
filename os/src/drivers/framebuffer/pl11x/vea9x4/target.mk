TARGET   = fb_drv
REQUIRES = pl11x platform_vea9x4
SRC_CC   = main.cc video_memory.cc
LIBS     = base
INC_DIR += $(PRG_DIR)/..

vpath main.cc $(PRG_DIR)/..
