TARGET   = nova_timer_drv
REQUIRES = nova
INC_DIR += $(REP_DIR)/src/drivers/timer/nova
SRC_CC  += nova/time_source.cc

include $(REP_DIR)/src/drivers/timer/target.inc
