TARGET   = rpi_gpio
REQUIRES = arm_v6
SRC_CC  += main.cc
LIBS     = base
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)
