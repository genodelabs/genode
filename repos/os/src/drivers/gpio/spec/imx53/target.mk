TARGET   = imx53_gpio_drv
REQUIRES = arm_v7
SRC_CC   = main.cc
LIBS     = base
INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/gpio/spec/imx

vpath main.cc $(REP_DIR)/src/drivers/gpio/spec/imx
