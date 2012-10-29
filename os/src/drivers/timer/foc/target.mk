TARGET   = timer
SRC_CC   = main.cc platform_timer.cc
REQUIRES = foc
LIBS     = cxx server env alarm signal

INC_DIR  = $(PRG_DIR)/../include $(PRG_DIR)/../include_periodic

vpath main.cc $(PRG_DIR)/..
vpath platform_timer.cc $(REP_DIR)/src/drivers/timer/fiasco
