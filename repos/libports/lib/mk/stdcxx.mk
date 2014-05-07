STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# public headers from port
INC_DIR += $(STDCXX_PORT_DIR)/include/stdcxx

# enable 'atomic.cc' to find 'gstdint.h'
INC_DIR += $(REP_DIR)/include/stdcxx/bits

## enable 'allocator-inst.cc' to find 'memory'
#INC_DIR += $(REP_DIR)/include/stdcxx/std

# exclude code that is no single compilation unit
FILTER_OUT = hash-long-double-tr1-aux.cc

# exclude deprecated parts
FILTER_OUT += strstream.cc

# add libstdc++ sources
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++11/*.cc)))
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++98/*.cc)))

# add config/locale/generic sources
SRC_CC += $(notdir $(wildcard $(STDCXX_DIR)/config/locale/generic/*.cc))

# add config/os/generic sources
SRC_CC += $(notdir $(wildcard $(STDCXX_DIR)/config/os/generic/*.cc))

CC_OPT += -D__GXX_EXPERIMENTAL_CXX0X__ -std=c++11

# add config/io backend
SRC_CC  += basic_file_stdio.cc
INC_DIR += $(STDCXX_DIR)/config/io

# add bits of libsupc++ (most parts are already contained in the cxx library)
SRC_CC  += new_op.cc new_opnt.cc new_opv.cc new_opvnt.cc new_handler.cc
SRC_CC  += del_op.cc del_opnt.cc del_opv.cc del_opvnt.cc
SRC_CC  += bad_cast.cc bad_alloc.cc bad_typeid.cc
SRC_CC  += eh_aux_runtime.cc hash_bytes.cc
SRC_CC  += tinfo.cc
INC_DIR += $(STDCXX_DIR)/libsupc++

include $(REP_DIR)/lib/import/import-stdcxx.mk

vpath %.cc $(STDCXX_DIR)/src/c++11
vpath %.cc $(STDCXX_DIR)/src/c++98
vpath %.cc $(STDCXX_DIR)/config/locale/generic
vpath %.cc $(STDCXX_DIR)/config/os/generic
vpath %.cc $(STDCXX_DIR)/config/io
vpath %.cc $(STDCXX_DIR)/libsupc++

SHARED_LIB = yes
