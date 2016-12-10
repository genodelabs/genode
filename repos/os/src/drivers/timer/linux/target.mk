TARGET   = linux_timer_drv
REQUIRES = linux
INC_DIR += $(REP_DIR)/src/drivers/timer/periodic
SRC_CC  += periodic/time_source.cc linux/time_source.cc
LIBS    += syscall-linux

include $(REP_DIR)/src/drivers/timer/target.inc
