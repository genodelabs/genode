TARGET   = pit_timer_drv
REQUIRES = x86
INC_DIR += $(PRG_DIR)
SRC_CC  += time_source.cc

include $(REP_DIR)/src/drivers/timer/target.inc
