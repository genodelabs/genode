TARGET   = uart_drv
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = cxx env server signal

INC_DIR += $(PRG_DIR) $(PRG_DIR)/..
