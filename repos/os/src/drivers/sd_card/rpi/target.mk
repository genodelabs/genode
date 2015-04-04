TARGET   = sd_card_drv
REQUIRES = platform_rpi
SRC_CC   = main.cc
LIBS     = base server
INC_DIR += $(PRG_DIR) $(PRG_DIR)/..
