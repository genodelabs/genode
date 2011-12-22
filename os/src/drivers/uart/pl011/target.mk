TARGET   = uart_drv
REQUIRES = pl011
SRC_CC   = main.cc
LIBS     = cxx env server signal

INC_DIR += $(PRG_DIR) $(PRG_DIR)/..
