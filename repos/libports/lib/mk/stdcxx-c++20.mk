STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

include $(REP_DIR)/lib/import/import-stdcxx.mk

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# add c++20 sources
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++20/*.cc)))

CC_CXX_OPT_STD = -std=gnu++20

vpath %.cc $(STDCXX_DIR)/src/c++20

CC_CXX_WARN_STRICT =
