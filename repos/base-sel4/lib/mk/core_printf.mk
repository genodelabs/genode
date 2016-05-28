SRC_CC   = core_printf.cc default_log.cc
INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include
LIBS    += syscall

vpath core_printf.cc $(BASE_DIR)/src/lib/base
vpath default_log.cc $(BASE_DIR)/src/core
