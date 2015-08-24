TARGET   = gpio_drv
REQUIRES = platform_rpi gpio
SRC_CC   = main.cc
LIBS     = base config server
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)
