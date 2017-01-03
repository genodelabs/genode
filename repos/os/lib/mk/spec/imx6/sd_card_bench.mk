INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/imx
SRC_CC  += adma2.cc
SRC_CC  += spec/imx/driver.cc
SRC_CC  += spec/imx6/driver.cc

vpath main.cc $(REP_DIR)/src/test/sd_card_bench

include $(REP_DIR)/lib/mk/sd_card.inc
