STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

include $(REP_DIR)/lib/import/import-stdcxx.mk

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# enable 'compatibility-atomic-c++0x.cc' to find 'gstdint.h'
REP_INC_DIR += include/stdcxx/bits

INC_DIR += $(STDCXX_DIR)/libsupc++

# add c++11 sources
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++11/*.cc)))

CC_CXX_OPT_STD = -std=gnu++11

vpath %.cc $(STDCXX_DIR)/src/c++11

CC_CXX_WARN_STRICT =
