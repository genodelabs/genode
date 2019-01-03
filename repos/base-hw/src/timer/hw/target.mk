TARGET   = hw_timer_drv
REQUIRES = hw
LIBS     = syscall-hw
INC_DIR += $(PRG_DIR)
SRC_CC  += time_source.cc

include $(call select_from_repositories,src/timer/target.inc)
