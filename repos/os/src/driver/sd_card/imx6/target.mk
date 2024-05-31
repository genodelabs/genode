TARGET   = imx6_sd_card
SRC_CC   = adma2.cc imx/driver.cc
INC_DIR  = $(REP_DIR)/src/driver/sd_card/imx
REQUIRES = arm_v7a

include $(REP_DIR)/src/driver/sd_card/target.inc
