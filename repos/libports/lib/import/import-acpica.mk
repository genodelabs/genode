INC_DIR += $(call select_from_ports,acpica)/src/lib/acpica/source/include
CC_OPT  += -DACPI_INLINE=inline -Wno-unused-variable
