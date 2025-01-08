LWIP_DIR := $(call select_from_ports,lwip)
INC_DIR += $(LWIP_DIR)/include/lwip
INC_DIR += $(call select_from_repositories,include/lwip)
