TARGET   = sd_card_bench
REQUIRES = imx53
SRC_CC   = main.cc
LIBS     = base server
INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/imx53
INC_DIR += $(REP_DIR)/src/drivers/sd_card
