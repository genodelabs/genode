LIBS += base

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/drivers/spi

SRC_CC += main.cc

vpath %.cc $(REP_DIR)/src/drivers/spi
vpath %.cc $(REP_DIR)
