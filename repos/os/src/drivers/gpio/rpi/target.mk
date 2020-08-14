TARGET   = rpi_gpio_drv
REQUIRES = arm_v6
SRC_CC  += main.cc
LIBS     = base
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)
