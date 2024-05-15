TARGET   = pit_timer_drv
REQUIRES = x86
SRC_CC  += main.cc
LIBS    += base

REP_INC_DIR += src/include

vpath main.cc $(dir $(call select_from_repositories,src/timer/pit/main.cc))
