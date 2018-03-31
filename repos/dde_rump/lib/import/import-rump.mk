RUMP_PORT_DIR := $(call select_from_ports,dde_rump)/src/lib/dde_rump
RUMP_BASE     := $(BUILD_BASE_DIR)/var/libcache/rump

ifeq ($(filter-out $(SPECS),arm),)
	# rump include shadows some parts of 'machine' on ARM only,
	# Therefore, it must be included before RUMP_BASE/include/machine
	INC_DIR := $(RUMP_PORT_DIR)/src/sys/rump/include $(INC_DIR)
endif

INC_DIR += $(LIBGCC_INC_DIR) \
           $(RUMP_PORT_DIR)/src/sys \
           $(RUMP_PORT_DIR)/src/sys/rump/include  \
           $(RUMP_PORT_DIR)/src/sys/sys \
           $(RUMP_PORT_DIR)/src/common/include \
           $(RUMP_BASE)/include

CC_CXX_WARN_STRICT =
