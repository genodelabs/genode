#
# Force to build -O2 on RISC-V in order to get proper inlining during self
# relocation
#
override CC_OLEVEL = -O2

include $(BASE_DIR)/lib/mk/ld-platform.inc

INC_DIR += $(DIR)/spec/riscv
vpath %.s $(DIR)/spec/riscv
