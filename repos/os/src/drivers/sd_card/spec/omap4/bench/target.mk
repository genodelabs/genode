TARGET   = sd_card_bench
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = base server
INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/omap4
INC_DIR += $(REP_DIR)/src/drivers/sd_card
