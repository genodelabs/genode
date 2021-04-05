STDCXX_INCLUDE_DIR := $(call select_from_repositories,include/stdcxx)

INC_DIR += $(STDCXX_INCLUDE_DIR) \
           $(STDCXX_INCLUDE_DIR)/std \
           $(STDCXX_INCLUDE_DIR)/c_global

STDCXX_PORT_INCLUDE_DIR := $(call select_from_ports,stdcxx)/include/stdcxx
INC_DIR += $(STDCXX_PORT_INCLUDE_DIR) \
           $(STDCXX_PORT_INCLUDE_DIR)/std \
           $(STDCXX_PORT_INCLUDE_DIR)/c_global

ifeq ($(filter-out $(SPECS),arm),)
	INC_DIR += $(STDCXX_INCLUDE_DIR)/../spec/arm/stdcxx
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	INC_DIR += $(STDCXX_INCLUDE_DIR)/../spec/arm_64/stdcxx
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(STDCXX_INCLUDE_DIR)/../spec/x86_32/stdcxx
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(STDCXX_INCLUDE_DIR)/../spec/x86_64/stdcxx
endif

# stdcxx headers include libc headers
include $(call select_from_repositories,lib/import/import-libc.mk)

# prevent gcc headers from defining mbstate
CC_OPT += -D_GLIBCXX_HAVE_MBSTATE_T

# use compiler-builtin atomic operations
CC_OPT += -D_GLIBCXX_ATOMIC_BUILTINS_4

# No isinf isnan
CC_OPT += -D_GLIBCXX_NO_OBSOLETE_ISINF_ISNAN_DYNAMIC
