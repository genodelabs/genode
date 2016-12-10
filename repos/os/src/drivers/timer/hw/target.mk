TARGET   = hw_timer_drv
REQUIRES = hw
LIBS     = syscall-hw
INC_DIR += $(PRG_DIR)
SRC_CC  += hw/time_source.cc

include $(REP_DIR)/src/drivers/timer/target.inc
