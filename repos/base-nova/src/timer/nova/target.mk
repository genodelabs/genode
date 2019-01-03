TARGET   = nova_timer_drv
INC_DIR += $(PRG_DIR)
SRC_CC  += time_source.cc

include $(call select_from_repositories,src/timer/target.inc)
