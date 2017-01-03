TARGET   = fb_drv
REQUIRES = pl11x pbxa9
SRC_CC   = main.cc
LIBS     = base
INC_DIR += $(PRG_DIR)/..

vpath %.cc $(PRG_DIR)/..
