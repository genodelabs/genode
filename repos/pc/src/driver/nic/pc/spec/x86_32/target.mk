REQUIRES := x86_32

SRC_C += lx_emul/spec/x86_32/atomic64_32.c
SRC_C += lx_emul/shadow/arch/x86/kernel/irq_32.c

include $(PRG_DIR)/../../target.inc
