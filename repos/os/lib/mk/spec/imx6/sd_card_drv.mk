INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/imx
SRC_CC  += spec/imx/adma2.cc
SRC_CC  += spec/imx/sdhc.cc
SRC_CC  += spec/imx/main.cc
SRC_CC  += spec/imx6/sdhc.cc
include $(REP_DIR)/lib/mk/sd_card.inc
