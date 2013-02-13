include $(REP_DIR)/lib/mk/timer.inc

INC_DIR += $(REP_DIR)/src/drivers/timer/include_periodic

#
# Supply dummy includes to prevent warning about missing string.h and stdio.h,
# which are included by Codezero's headers.
#
REP_INC_DIR += include/codezero/dummies

SRC_CC += platform_timer.cc
vpath platform_timer.cc $(REP_DIR)/src/drivers/timer/codezero

