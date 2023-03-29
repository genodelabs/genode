SRC_NOLINK += atomic_add_32.S atomic_add_64.S atomic_and_32.S atomic_and_64.S \
              atomic_cas_32.S atomic_cas_64.S atomic_nand_32.S atomic_nand_64.S \
              atomic_or_32.S atomic_or_64.S atomic_sub_32.S atomic_sub_64.S \
              atomic_swap_32.S atomic_swap_64.S atomic_xor_32.S atomic_xor_64.S \
              membar_ops.S

SRC_NOLINK += rump_generic_cpu.c rump_generic_pmap.c

INC_DIR += $(RUMP_PORT_DIR)/src/sys/rump/include
#INC_DIR += $(REP_DIR)/src/lib/rump/spec/arm_64

include $(REP_DIR)/lib/mk/rump.inc

vpath %.S $(RUMP_PORT_DIR)/../libc/riscv/atomic
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/gen
vpath %.c $(RUMP_PORT_DIR)/../dde_rump_backport/riscv/riscv
vpath %.c $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern/arch/generic

CC_CXX_WARN_STRICT =
