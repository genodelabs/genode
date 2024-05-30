TARGET    = imx8q_evk_i2c

REQUIRES  = arm_v8

SRC_CC    += driver.cc

INC_DIR   += $(PRG_DIR)/src/drivers/i2c/imx8q_evk

include $(REP_DIR)/src/drivers/i2c/target.inc
