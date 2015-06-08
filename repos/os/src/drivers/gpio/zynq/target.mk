# \brief  Gpio specific for zynq systems
# \author Mark Albers
# \date   2015-03-30

TARGET   = zynq_gpio_drv

SRC_CC   = main.cc
LIBS     = base config libc libm stdcxx
INC_DIR += $(PRG_DIR)

vpath main.cc $(PRG_DIR)

