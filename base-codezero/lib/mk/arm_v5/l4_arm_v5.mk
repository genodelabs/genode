LIBL4_DIR = $(CODEZERO_DIR)/conts/userlibs/libl4

INC_DIR += $(CODEZERO_DIR)/conts/userlibs/libc/include

SRC_C += $(notdir $(wildcard $(LIBL4_DIR)/src/arch/arm/v5/*.c))
SRC_S += $(notdir $(wildcard $(LIBL4_DIR)/src/arch/arm/v5/*.S))

vpath %.c $(LIBL4_DIR)/src/arch/arm/v5
vpath %.S $(LIBL4_DIR)/src/arch/arm/v5
