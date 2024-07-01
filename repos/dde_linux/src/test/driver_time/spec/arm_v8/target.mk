REQUIRES := arm_v8

SRC_CC += timestamp.cc
SRC_C  += lx_emul.c
SRC_C  += lx_emul/shadow/arch/arm64/kernel/smp.c

include $(PRG_DIR)/../../target.inc
