INC_DIR += $(REP_DIR)/src/drivers/sd_card/spec/imx
SRC_CC  += adma2.cc
SRC_CC  += spec/imx/driver.cc
SRC_CC  += spec/imx53/driver.cc

include $(REP_DIR)/lib/mk/sd_card.inc
