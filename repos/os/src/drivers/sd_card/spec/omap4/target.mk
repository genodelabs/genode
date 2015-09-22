TARGET   = sd_card_drv
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = base server
INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/sd_card
