SRC_CC  += main.cc
LIBS    += cxx server env alarm signal
INC_DIR += $(REP_DIR)/src/drivers/timer/include

vpath main.cc $(REP_DIR)/src/drivers/timer
