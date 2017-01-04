INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/imx53
SRC_CC  += spec/imx53/adma2.cc
SRC_CC  += spec/imx53/esdhcv2.cc
SRC_CC  += spec/imx53/main.cc
include $(REP_DIR)/lib/mk/sd_card.inc
