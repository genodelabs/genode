SRC_CC   = main.cc
LIBS     = base

INC_DIR += $(PRG_DIR) $(REP_DIR)/src/drivers/uart

vpath main.cc $(REP_DIR)/src/drivers/uart
