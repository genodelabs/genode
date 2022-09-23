PCGC_DIR := $(call select_from_ports,pcg-c)
INC_DIR += \
	$(PCGC_DIR)/include/pcg-c \
	$(call select_from_repositories,include/pcg-c)
