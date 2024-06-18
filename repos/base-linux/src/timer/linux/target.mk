TARGET   = linux_timer
INC_DIR += $(PRG_DIR)
SRC_CC  += component.cc
LIBS    += base syscall-linux

REP_INC_DIR += src/include
