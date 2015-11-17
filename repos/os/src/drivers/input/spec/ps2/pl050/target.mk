TARGET   = ps2_drv
REQUIRES = pl050
SRC_CC   = main.cc
LIBS     = base server config

INC_DIR += $(PRG_DIR) $(PRG_DIR)/..
