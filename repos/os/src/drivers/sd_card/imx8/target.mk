TARGET   = imx8_sd_card_drv
SRC_CC   = adma2.cc imx/driver.cc
INC_DIR  = $(REP_DIR)/src/drivers/sd_card/imx
REQUIRES = arm_v8a

include $(REP_DIR)/src/drivers/sd_card/target.inc

vpath driver.cc  $(REP_DIR)/src/drivers/sd_card/imx6

