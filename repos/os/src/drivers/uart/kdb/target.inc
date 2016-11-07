TARGET   = kdb_uart_drv
SRC_CC   = main.cc
LIBS     = base config
INC_DIR += $(REP_DIR)/src/drivers/uart $(REP_DIR)/src/drivers/uart/kdb

vpath main.cc $(REP_DIR)/src/drivers/uart
