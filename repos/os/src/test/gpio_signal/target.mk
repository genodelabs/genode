TARGET   = signal_gpio_drv
REQUIRES = gpio
SRC_CC   = main.cc
INC_DIR += $(PRG_DIR)
LIBS     = base config

vpath main.cc $(PRG_DIR)
