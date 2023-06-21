SRC_CC += component.cc
SRC_CC += default_log.cc
SRC_CC += platform.cc stack_area.cc
SRC_CC += rpc_cap_alloc.cc heartbeat.cc
SRC_CC += vm.cc

vpath %.cc  $(REP_DIR)/src/lib/base
vpath %.cc $(BASE_DIR)/src/lib/base

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include
