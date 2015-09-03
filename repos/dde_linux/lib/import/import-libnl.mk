NL_CONTRIB_INC_DIR := $(call select_from_ports,dde_linux)/src/lib/libnl
INC_DIR += $(NL_CONTRIB_INC_DIR)/include

LIBNL_INC_DIR = $(REP_DIR)/src/lib/libnl/include
INC_DIR += $(LIBNL_INC_DIR)
ifneq ($(filter 32bit,$(SPECS)),)
	INC_DIR += $(LIBNL_INC_DIR)/spec/32bit
endif
ifneq ($(filter 64bit,$(SPECS)),)
	INC_DIR += $(LIBNL_INC_DIR)/spec/64bit
endif
