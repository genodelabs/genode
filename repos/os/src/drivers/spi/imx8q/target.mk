TARGET    = imx8q_spi_drv

REQUIRES  = arm_v8

SRC_CC    += ecspi/ecspi_driver.cc \
             imx8q_spi_initializer.cc

INC_DIR   += $(PRG_DIR)/src/drivers/spi/imx8q

include $(REP_DIR)/src/drivers/spi/target.inc
