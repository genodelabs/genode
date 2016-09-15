SPECS       += hw riscv platform_riscv 64bit
NR_OF_CPUS   = 1
REP_INC_DIR += include/spec/riscv

include $(call select_from_repositories,mk/spec/64bit.mk)
include $(call select_from_repositories,mk/spec/hw.mk)
