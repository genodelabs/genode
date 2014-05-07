GMP_PORT_DIR := $(call select_from_ports,gmp)

REP_INC_DIR += include/gmp

INC_DIR += $(GMP_PORT_DIR)/include

ifeq ($(filter-out $(SPECS),arm),)
	REP_INC_DIR += include/gmp/32bit
	INC_DIR += $(GMP_PORT_DIR)/include/arm
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	REP_INC_DIR += include/gmp/32bit
	INC_DIR += $(GMP_PORT_DIR)/include/x86_32
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	REP_INC_DIR += include/gmp/64bit
	INC_DIR += $(GMP_PORT_DIR)/include/x86_64
endif
