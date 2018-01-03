#
# x86 specific
#
SRC_NOLINK += atomic.S byte_swap_2.S byte_swap_4.S byte_swap_8.S

SRC_NOLINK += rump_x86_cpu_counter.c \
              rump_x86_cpu.c \
              rump_x86_pmap.c \
              rump_x86_spinlock.c \
              rump_x86_spl.c

INC_MACHINE = $(RUMP_PORT_DIR)/src/sys/arch/amd64/include
INC_ARCH    = x86

include $(REP_DIR)/lib/mk/rump.inc

vpath %.S $(RUMP_PORT_DIR)/src/common/lib/libc/arch/x86_64/atomic
vpath %.S $(RUMP_PORT_DIR)/src/common/lib/libc/arch/x86_64/gen
vpath %.c $(RUMP_PORT_DIR)/src/sys/arch/amd64/amd64
vpath %.c $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern/arch/x86

CC_CXX_WARN_STRICT =
