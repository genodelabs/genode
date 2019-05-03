TARGET   = imx53_sd_card_drv
REQUIRES = arm_v7
SRC_CC   = adma2.cc spec/imx/driver.cc
INC_DIR  = $(REP_DIR)/src/drivers/sd_card/spec/imx

include $(REP_DIR)/src/drivers/sd_card/target.inc
