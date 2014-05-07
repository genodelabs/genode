SRC_CC  += main.cc
LIBS    += base alarm
INC_DIR += $(REP_DIR)/src/drivers/timer/include

vpath main.cc $(REP_DIR)/src/drivers/timer
