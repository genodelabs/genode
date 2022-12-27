FFI_DIR := $(call select_from_ports,ffi)

ifeq ($(filter-out $(SPECS),x86_32),)
INC_DIR += $(FFI_DIR)/include/ffi/spec/x86_32
endif

ifeq ($(filter-out $(SPECS),x86_64),)
INC_DIR += $(FFI_DIR)/include/ffi/spec/x86_64
endif

ifeq ($(filter-out $(SPECS),arm),)
INC_DIR += $(FFI_DIR)/include/ffi/spec/arm
endif

ifeq ($(filter-out $(SPECS),arm_64),)
INC_DIR += $(FFI_DIR)/include/ffi/spec/arm_64
endif
