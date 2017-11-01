LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/nic/fec
SRC_DIR        := $(REP_DIR)/src/drivers/nic/fec
INC_DIR        += $(LX_CONTRIB_DIR)/drivers/net/ethernet/freescale
INC_DIR        += $(LIB_CACHE_DIR)/fec_nic_include/include/include/include
CC_OPT         += -U__linux__ -D__KERNEL__
