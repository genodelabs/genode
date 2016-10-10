SRC_CC   = core_log.cc default_log.cc
INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include $(BASE_DIR)/src/core/include
LIBS    += syscall

vpath core_log.cc    $(REP_DIR)/src/core
vpath default_log.cc $(BASE_DIR)/src/core
