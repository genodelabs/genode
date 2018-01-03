#
# x86 specific
#
SRC_NOLINK += atomic.S bswap64.c

SRC_NOLINK += rump_x86_cpu_counter.c \
              rump_x86_cpu.c \
              rump_x86_pmap.c \
              rump_x86_spinlock.c \
              rump_x86_spl.c


include $(REP_DIR)/lib/mk/rump.inc

vpath %.S $(RUMP_PORT_DIR)/src/common/lib/libc/arch/i386/atomic
vpath %.S $(RUMP_PORT_DIR)/src/common/lib/libc/gen
vpath %.c $(RUMP_PORT_DIR)/src/sys/arch/i386/i386
vpath %.c $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern/arch/x86

CC_CXX_WARN_STRICT =
