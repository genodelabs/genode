TARGET   = imx8q_evk_sd_card_drv
REQUIRES = arm_v8
SRC_CC  += adma2.cc spec/imx/driver.cc spec/imx6/driver.cc
INC_DIR  = $(REP_DIR)/src/drivers/sd_card/spec/imx

include $(REP_DIR)/src/drivers/sd_card/target.inc
