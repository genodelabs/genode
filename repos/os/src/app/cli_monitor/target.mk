TARGET   = cli_monitor
SRC_CC   = main.cc
LIBS     = base cli_monitor config vfs
INC_DIR += $(PRG_DIR)

ifeq ($(findstring arm, $(SPECS)), arm)
INC_DIR += $(PRG_DIR)/spec/arm
endif

ifeq ($(findstring x86, $(SPECS)), x86)
INC_DIR += $(PRG_DIR)/spec/x86
endif

ifeq ($(findstring riscv, $(SPECS)), riscv)
INC_DIR += $(PRG_DIR)/spec/riscv
endif
