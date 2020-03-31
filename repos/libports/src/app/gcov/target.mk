TARGET = gcov

GCOV_PORT_DIR := $(call select_from_ports,gcov)

GCOV_DIR := $(GCOV_PORT_DIR)/src/gcov

SRC_CC = gcov.cc

CC_OPT_gcov = -DIN_GCC -DHAVE_CONFIG_H

INC_DIR += $(GCOV_DIR)/gcc
INC_DIR += $(GCOV_DIR)/include
INC_DIR += $(GCOV_DIR)/libcpp/include

ifeq ($(filter-out $(SPECS),arm),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm/gcc
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm_64/gcc
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_32/gcc
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_64/gcc
endif

LIBS += libc posix gmp stdcxx gcov-libcommon gcov-libcpp gcov-libiberty

vpath %.cc $(GCOV_DIR)/gcc

CC_CXX_WARN_STRICT =
