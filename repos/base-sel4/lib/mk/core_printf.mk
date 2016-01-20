SRC_CC   = core_printf.cc
INC_DIR += $(REP_DIR)/src/include
LIBS    += syscall

vpath core_printf.cc $(BASE_DIR)/src/base/console
