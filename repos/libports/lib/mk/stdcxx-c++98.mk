STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

include $(REP_DIR)/lib/import/import-stdcxx.mk

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# exclude code that is no single compilation unit
FILTER_OUT = hash-long-double-tr1-aux.cc

# exclude deprecated parts
FILTER_OUT += strstream.cc

# add libstdc++ sources
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++98/*.cc)))

CC_CXX_OPT_STD             = -std=gnu++98
CC_CXX_OPT_STD_locale_init = -std=gnu++11
CC_CXX_OPT_STD_localename  = -std=gnu++11

CC_OPT_collate_members_cow  += -D_GLIBCXX_USE_CXX11_ABI=0
CC_OPT_messages_members_cow += -D_GLIBCXX_USE_CXX11_ABI=0
CC_OPT_monetary_members_cow += -D_GLIBCXX_USE_CXX11_ABI=0
CC_OPT_numeric_members_cow  += -D_GLIBCXX_USE_CXX11_ABI=0

# add config/cpu/generic sources
SRC_CC += $(notdir $(wildcard $(STDCXX_DIR)/config/cpu/generic/*.cc))

# add config/locale/generic sources
SRC_CC += $(notdir $(wildcard $(STDCXX_DIR)/config/locale/generic/*.cc))

# add config/os/generic sources
SRC_CC += $(notdir $(wildcard $(STDCXX_DIR)/config/os/generic/*.cc))

# add config/io backend
SRC_CC  += basic_file_stdio.cc
INC_DIR += $(STDCXX_DIR)/config/io

vpath %.cc $(STDCXX_DIR)/src/c++98
vpath %.cc $(STDCXX_DIR)/config/cpu/generic
vpath %.cc $(STDCXX_DIR)/config/locale/generic
vpath %.cc $(STDCXX_DIR)/config/os/generic
vpath %.cc $(STDCXX_DIR)/config/io
