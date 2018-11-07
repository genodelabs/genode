SOLO5_PORT_DIR := $(call select_from_ports,solo5)

INC_DIR += $(SOLO5_PORT_DIR)/include/solo5
INC_DIR += $(call select_from_repositories,/include/solo5)
