INC_DIR += $(REP_DIR)/src/drivers/timer/spec/periodic

SRC_CC += spec/periodic/time_source.cc spec/linux/time_source.cc

LIBS += syscall

include $(REP_DIR)/lib/mk/timer.inc
