TARGET   = fiasco_timer_drv
REQUIRES = fiasco
LIBS    += syscall-fiasco
INC_DIR += $(REP_DIR)/src/drivers/timer/periodic
SRC_CC  += periodic/time_source.cc fiasco/time_source.cc

include $(REP_DIR)/src/drivers/timer/target.inc
