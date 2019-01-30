SRC_CC += default_log.cc
SRC_CC += env_deprecated.cc stack_area.cc env_reinit.cc main_thread_cap.cc
SRC_CC += rpc_cap_alloc.cc heartbeat.cc

vpath %.cc  $(REP_DIR)/src/lib/base
vpath %.cc $(BASE_DIR)/src/lib/base

INC_DIR += $(REP_DIR)/src/include $(BASE_DIR)/src/include
