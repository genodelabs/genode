INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/imx
SRC_CC  += spec/imx/adma2.cc
SRC_CC  += spec/imx/sdhc.cc
SRC_CC  += spec/imx53/sdhc.cc

vpath main.cc $(REP_DIR)/src/test/sd_card_bench

include $(REP_DIR)/lib/mk/sd_card.inc
