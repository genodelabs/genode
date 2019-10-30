# this is a static library, because some symbols are marked as hidden

GCOV_PORT_DIR := $(call select_from_ports,gcov)

GCOV_DIR := $(GCOV_PORT_DIR)/src/gcov

SRC_C = libgcov-merge.c \
        libgcov-profiler.c \
        libgcov-interface.c \
        libgcov-driver.c

SRC_CC = libc.cc

LIBGCOV_MERGE = _gcov_merge_add \
                _gcov_merge_single \
                _gcov_merge_delta \
                _gcov_merge_ior \
                _gcov_merge_time_profile \
                _gcov_merge_icall_topn

LIBGCOV_PROFILER = _gcov_interval_profiler \
                   _gcov_pow2_profiler \
                   _gcov_one_value_profiler \
                   _gcov_indirect_call_profiler	\
                   _gcov_average_profiler \
                   _gcov_ior_profiler \
                   _gcov_indirect_call_profiler_v2 \
                   _gcov_time_profiler \
                   _gcov_indirect_call_topn_profiler

LIBGCOV_INTERFACE = _gcov_dump \
                    _gcov_flush \
                    _gcov_reset

LIBGCOV_DRIVER = _gcov

CC_OPT += -fbuilding-libgcc -DIN_GCC -DIN_LIBGCC2

CC_OPT += $(addprefix -DL,$(LIBGCOV_MERGE))
CC_OPT += $(addprefix -DL,$(LIBGCOV_PROFILER))
CC_OPT += $(addprefix -DL,$(LIBGCOV_INTERFACE))
CC_OPT += $(addprefix -DL,$(LIBGCOV_DRIVER))

INC_DIR += $(GCOV_DIR)/include \
           $(GCOV_DIR)/gcc \
           $(REP_DIR)/src/lib/gcov/libc

ifeq ($(filter-out $(SPECS),arm),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm/gcc \
	           $(GCOV_PORT_DIR)/include/arm/libgcc
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm_64/gcc \
	           $(GCOV_PORT_DIR)/include/arm_64/libgcc
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_32/gcc \
	           $(GCOV_PORT_DIR)/include/x86_32/libgcc
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_64/gcc \
	           $(GCOV_PORT_DIR)/include/x86_64/libgcc
endif

vpath %.c $(GCOV_DIR)/libgcc
vpath libc.cc $(REP_DIR)/src/lib/gcov/libc

CC_CXX_WARN_STRICT =
