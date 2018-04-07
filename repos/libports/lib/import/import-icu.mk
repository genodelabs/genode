ifeq ($(CONTRIB_DIR),)
ICU_INC_DIR := $(call select_from_repositories,include/icu)
else
ICU_INC_DIR := $(call select_from_ports,icu)/include/icu
endif

INC_DIR += $(ICU_INC_DIR)/common
INC_DIR += $(ICU_INC_DIR)/i18n
