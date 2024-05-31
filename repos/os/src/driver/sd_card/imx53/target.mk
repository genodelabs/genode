TARGET   = imx53_sd_card
REQUIRES = arm_v7
SRC_CC   = adma2.cc imx/driver.cc
INC_DIR  = $(REP_DIR)/src/driver/sd_card/imx

include $(REP_DIR)/src/driver/sd_card/target.inc
