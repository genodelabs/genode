ICU_PORT_DIR := $(call select_from_ports,icu)

INC_DIR += $(ICU_PORT_DIR)/include/icu/common
INC_DIR += $(ICU_PORT_DIR)/include/icu/i18n
