GMP_PORT_DIR := $(call select_from_ports,gmp)

REP_INC_DIR += include/gmp

INC_DIR += $(GMP_PORT_DIR)/include

ifeq ($(filter-out $(SPECS),arm),)
	REP_INC_DIR += include/spec/32bit/gmp
	REP_INC_DIR += include/spec/arm/gmp
	INC_DIR += $(GMP_PORT_DIR)/include/spec/arm
endif

ifeq ($(filter-out $(SPECS),arm_64),)
	REP_INC_DIR += include/spec/64bit/gmp
	REP_INC_DIR += include/spec/arm_64/gmp
	INC_DIR += $(GMP_PORT_DIR)/include/spec/arm_64
endif
ifeq ($(filter-out $(SPECS),x86_32),)
	REP_INC_DIR += include/spec/32bit/gmp
	REP_INC_DIR += include/spec/x86_32/gmp
	INC_DIR += $(GMP_PORT_DIR)/include/spec/x86_32
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	REP_INC_DIR += include/spec/64bit/gmp
	REP_INC_DIR += include/spec/x86_64/gmp
	INC_DIR += $(GMP_PORT_DIR)/include/spec/x86_64
endif
