TARGET   = pistachio_timer_drv
REQUIRES = pistachio
INC_DIR += $(REP_DIR)/src/drivers/timer/include
INC_DIR += $(REP_DIR)/src/drivers/timer/periodic
SRC_CC  += periodic/time_source.cc pistachio/time_source.cc main.cc
LIBS    += syscall-pistachio base-pistachio timeout

vpath %.cc $(REP_DIR)/src/drivers/timer
