TARGET   = gpio_drv
REQUIRES = imx6
SRC_CC   = main.cc
LIBS     = base
INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/gpio/spec/imx

vpath main.cc $(REP_DIR)/src/drivers/gpio/spec/imx
