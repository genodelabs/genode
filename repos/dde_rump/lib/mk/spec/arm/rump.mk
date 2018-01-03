#
# ARM specific
#
SRC_NOLINK += atomic_inc_32.S atomic_swap.S atomic_add_32.S \
              atomic_or_32.S atomic_dec_32.S atomic_and_32.S \
              atomic_cas_32.S membar_ops.S \
              bswap64.c

SRC_NOLINK += rump_generic_cpu.c rump_generic_pmap.c

INC_DIR += $(RUMP_PORT_DIR)/src/sys/rump/include

include $(REP_DIR)/lib/mk/rump.inc

vpath %.S $(RUMP_PORT_DIR)/src/common/lib/libc/arch/arm/atomic
vpath %.S $(RUMP_PORT_DIR)/src/common/lib/libc/gen
vpath %.c $(RUMP_PORT_DIR)/src/sys/arch/arm/arm32
vpath %.c $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern/arch/generic

CC_CXX_WARN_STRICT =
