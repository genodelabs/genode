STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

include $(REP_DIR)/lib/import/import-stdcxx.mk

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# enable 'compatibility-atomic-c++0x.cc' to find 'gstdint.h'
REP_INC_DIR += include/stdcxx/bits

# add libstdc++ sources
SRC_CC += $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(STDCXX_DIR)/src/c++11/*.cc)))

# add bits of libsupc++ (most parts are already contained in the cxx library)
SRC_CC  += new_op.cc new_opnt.cc new_opv.cc new_opvnt.cc new_handler.cc
SRC_CC  += del_op.cc del_opnt.cc del_ops.cc del_opv.cc del_opvnt.cc del_opvs.cc
SRC_CC  += bad_array_length.cc bad_array_new.cc bad_cast.cc bad_alloc.cc bad_typeid.cc
SRC_CC  += eh_aux_runtime.cc hash_bytes.cc
SRC_CC  += tinfo.cc
INC_DIR += $(STDCXX_DIR)/libsupc++

LIBS += stdcxx-c++98 libc libm

vpath %.cc $(STDCXX_DIR)/src/c++11
vpath %.cc $(STDCXX_DIR)/libsupc++

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
