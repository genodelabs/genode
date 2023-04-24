STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

include $(REP_DIR)/lib/import/import-stdcxx.mk

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# add c++17 sources
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++17/*.cc)))

CC_CXX_OPT_STD = -std=gnu++17

vpath %.cc $(STDCXX_DIR)/src/c++17

CC_CXX_WARN_STRICT =
