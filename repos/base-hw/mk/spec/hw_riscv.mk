SPECS += hw riscv platform_riscv 64bit

LD_TEXT_ADDR      ?= 0x1000000
CORE_LD_TEXT_ADDR  = 0x200
NR_OF_CPUS         = 1
REP_INC_DIR       += include/spec/riscv

include $(call select_from_repositories,mk/spec/64bit.mk)
include $(call select_from_repositories,mk/spec/hw.mk)
