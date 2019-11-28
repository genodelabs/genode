SRC_NOLINK += atomic_add_16.S atomic_add_8.S atomic_and_64.S atomic_cas_32.S atomic_dec_32.S \
              atomic_inc_64.S atomic_nand_64.S atomic_or_32.S atomic_sub_16.S atomic_sub_8.S \
              atomic_swap_64.S atomic_xor_32.S membar_ops.S atomic_add_32.S atomic_and_16.S \
              atomic_and_8.S atomic_cas_64.S atomic_dec_64.S atomic_nand_16.S atomic_nand_8.S \
              atomic_or_64.S atomic_sub_32.S atomic_swap_16.S atomic_swap_8.S atomic_xor_64.S \
              atomic_add_64.S atomic_and_32.S atomic_cas_16.S atomic_cas_8.S atomic_inc_32.S \
              atomic_nand_32.S atomic_or_16.S atomic_or_8.S atomic_sub_64.S atomic_swap_32.S \
              atomic_xor_16.S atomic_xor_8.S bswap64.c

SRC_NOLINK += rump_generic_cpu.c rump_generic_pmap.c

INC_DIR += $(RUMP_PORT_DIR)/src/sys/rump/include
INC_DIR += $(REP_DIR)/src/lib/rump/spec/arm_64

include $(REP_DIR)/lib/mk/rump.inc

vpath %.S $(RUMP_PORT_DIR)/../libc
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/gen
vpath %.c $(RUMP_PORT_DIR)/../dde_rump_aarch64_backport/aarch64/aarch64
vpath %.c $(RUMP_PORT_DIR)/../dde_rump_aarch64_backport/arm/arm32
vpath %.c $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern/arch/generic

CC_CXX_WARN_STRICT =
