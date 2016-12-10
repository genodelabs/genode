SRC_CC  += main.cc
LIBS    += base timeout
INC_DIR += $(REP_DIR)/src/drivers/timer/include

vpath %.cc $(REP_DIR)/src/drivers/timer
