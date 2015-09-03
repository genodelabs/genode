TARGET   = uart_drv
REQUIRES = omap4
SRC_CC   = main.cc
LIBS     = base config

INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/uart
