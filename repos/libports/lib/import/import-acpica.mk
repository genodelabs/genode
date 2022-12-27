ACPICA_DIR := $(call select_from_ports,acpica)
INC_DIR += $(ACPICA_DIR)/src/lib/acpica/source/include
CC_OPT  += -DACPI_INLINE=inline -Wno-builtin-declaration-mismatch
