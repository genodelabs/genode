TARGET   = uart_drv
REQUIRES = pl011
SRC_CC   = main.cc
LIBS     = base config

INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/uart
