TARGET   = pbxa9_fb_drv
REQUIRES = arm_v7
SRC_CC   = main.cc
LIBS     = base
INC_DIR += $(PRG_DIR)

vpath %.cc $(PRG_DIR)/..
