TARGET   = led_gpio_drv
REQUIRES = gpio
SRC_CC   = main.cc
LIBS     = base config

vpath main.cc $(PRG_DIR)
