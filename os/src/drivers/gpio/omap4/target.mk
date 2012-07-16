
TARGET   = omap4_gpio_drv
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = cxx env server signal
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)

