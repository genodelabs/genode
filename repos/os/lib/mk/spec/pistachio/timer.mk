INC_DIR += $(REP_DIR)/src/drivers/timer/spec/periodic

SRC_CC += spec/periodic/time_source.cc spec/pistachio/time_source.cc

include $(REP_DIR)/lib/mk/timer.inc
