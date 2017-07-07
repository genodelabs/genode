TARGET   = wand_quad_timer_drv
REQUIRES = wand_quad
INC_DIR += $(REP_DIR)/src/drivers/timer/epit
SRC_CC  += time_source.cc
SRC_CC  += timer.cc

include $(REP_DIR)/src/drivers/timer/target.inc

vpath time_source.cc $(REP_DIR)/src/drivers/timer/epit
