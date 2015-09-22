TARGET   = kdb_uart_drv
SRC_CC   = main.cc
LIBS     = base config

INC_DIR += $(PRG_DIR)/../.. $(PRG_DIR)/../../..

vpath main.cc $(PRG_DIR)/../..
