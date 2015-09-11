REQUIRES = riscv

include $(REP_DIR)/lib/mk/ldso.inc

INC_DIR += $(DIR)/spec/riscv
vpath %.s $(DIR)/spec/riscv
