LIBS    += base

INC_DIR += $(PRG_DIR)
INC_DIR += $(REP_DIR)/src/driver/i2c

SRC_CC  += main.cc

vpath %.cc $(REP_DIR)/src/driver/i2c
vpath %.cc $(REP_DIR)
