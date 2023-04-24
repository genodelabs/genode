STDCXX_PORT_DIR := $(call select_from_ports,stdcxx)

include $(REP_DIR)/lib/import/import-stdcxx.mk

# determine location of libstdc++ source tree
STDCXX_DIR := $(STDCXX_PORT_DIR)/src/lib/stdcxx

# add bits of libsupc++ (most parts are already contained in the cxx library)
SRC_CC  += new_op.cc new_opnt.cc new_opv.cc new_opvnt.cc new_handler.cc
SRC_CC  += new_opa.cc new_opant.cc new_opva.cc new_opvant.cc
SRC_CC  += del_op.cc del_opnt.cc del_ops.cc del_opv.cc del_opvnt.cc del_opvs.cc
SRC_CC  += del_opa.cc del_opant.cc del_opsa.cc del_opva.cc del_opvant.cc del_opvsa.cc
SRC_CC  += bad_array_length.cc bad_array_new.cc bad_cast.cc bad_alloc.cc bad_typeid.cc
SRC_CC  += eh_aux_runtime.cc eh_ptr.cc hash_bytes.cc
SRC_CC  += tinfo.cc
INC_DIR += $(STDCXX_DIR)/libsupc++

LIBS += stdcxx-c++98 stdcxx-c++11 stdcxx-c++17 stdcxx-c++20 libc libm

vpath %.cc $(STDCXX_DIR)/libsupc++

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
