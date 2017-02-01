TARGET   = hw_gpio_drv
REQUIRES = rpi
SRC_CC  += ../main.cc
LIBS    += base config server
INC_DIR += $(PRG_DIR) $(PRG_DIR)/..
