include $(REP_DIR)/lib/mk/timer.inc

INC_DIR += $(REP_DIR)/src/drivers/timer/include_periodic

LIBS += syscall

SRC_CC += platform_timer.cc
vpath platform_timer.cc $(REP_DIR)/src/drivers/timer/linux
