TARGET   = sd_card_drv
REQUIRES = pl180
SRC_CC   = main.cc
LIBS     = cxx env server signal

INC_DIR += $(PRG_DIR) $(PRG_DIR)/..
