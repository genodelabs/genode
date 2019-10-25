GCOV_PORT_DIR := $(call select_from_ports,gcov)

GCOV_DIR := $(GCOV_PORT_DIR)/src/gcov

SRC_CC = line-map.cc

LIBS += libc

INC_DIR += $(GCOV_DIR)/include \
           $(GCOV_DIR)/libcpp/include

ifeq ($(filter-out $(SPECS),arm),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm/libcpp
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/arm_64/libcpp
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_32/libcpp
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(GCOV_PORT_DIR)/include/x86_64/libcpp
endif

vpath %.cc $(GCOV_DIR)/libcpp

CC_CXX_WARN_STRICT =
