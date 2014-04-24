# FreeBSD libc code
SRC_C += gen/ldexp.c
SRC_C += gen/usleep.c
SRC_C += stdio/ferror.c
SRC_C += stdio/fflush.c
SRC_C += stdio/fileno.c
SRC_C += stdio/findfp.c
SRC_C += stdio/stdio.c
SRC_C += string/memchr.c
SRC_C += string/strncmp.c
SRC_C += string/strdup.c

# Genode libc code
SRC_CC += libc/clock_gettime.cc
SRC_CC += libc/nanosleep.cc
SRC_CC += libc/file_operations.cc
SRC_CC += libc/plugin_registry.cc
SRC_CC += libc/fd_alloc.cc
SRC_CC += libc/libc_mem_alloc.cc
SRC_CC += libc/gettimeofday.cc
SRC_CC += libc/plugin.cc
SRC_CC += libc/select.cc

# Genode terminal plugin
SRC_CC += libc_terminal/plugin.cc

# Genode vfs plugin
SRC_CC += libc/vfs_plugin.cc
SRC_CC += libc/pread_pwrite.cc

# Genode lock pipe plugin (needed by VirtualBox "HostSerial" driver)
SRC_CC += libc_lock_pipe/plugin.cc

# Genode pthread code
SRC_CC += pthread/semaphore.cc
SRC_CC += pthread/thread.cc
LIBS   += timed_semaphore

# setjmp/longjmp needed by recompiler
LIBS   += libc-setjmp

INC_DIR += $(LIBC_REP_DIR)/src/lib/libc

vpath %.cc $(LIBC_REP_DIR)/src/lib

# FreeBSD libc FPU math
FPU_SRC_C = $(wildcard $(LIBC_DIR)/msun/src/*.c) \
            $(wildcard $(LIBC_DIR)/msun/ld80/*.c) \
            $(wildcard $(LIBC_DIR)/msun/bsdsrc/*.c)
SRC_C += $(filter-out e_rem_pio2.c e_rem_pio2f.c s_exp2l.c, $(notdir $(FPU_SRC_C)))

vpath %.c $(LIBC_DIR)/msun/src
vpath %.c $(LIBC_DIR)/msun/ld80
vpath %.c $(LIBC_DIR)/msun/bsdsrc

# Disable warnings for selected files, i.e., to suppress
# 'is static but used in inline function which is not static'
# messages
CC_OPT_s_tanf = -w
CC_OPT_s_tan  = -w
CC_OPT_s_sin  = -w
CC_OPT_s_cos  = -w
CC_OPT_s_cosf = -w
CC_OPT_s_sinf = -w
CC_OPT_k_cosf = -w
CC_OPT_k_sinf = -w
CC_OPT_k_tanf = -w

# Work-around to get over doubly defined symbols produced by several sources
# that include 'e_rem_pio2.c' and 'e_rem_pio2f.c'. To avoid symbol clashes,
# we rename each occurrence by adding the basename of the compilation unit
# as suffix. (copied from libm.mk)
CC_OPT_s_sin  += -D__ieee754_rem_pio2=__ieee754_rem_pio2_s_sin
CC_OPT_s_cos  += -D__ieee754_rem_pio2=__ieee754_rem_pio2_s_cos
CC_OPT_s_tan  += -D__ieee754_rem_pio2=__ieee754_rem_pio2_s_tan
CC_OPT_s_sinf += -D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_sinf
CC_OPT_s_sinf += -D__kernel_cosdf=__kernel_cosdf_sinf
CC_OPT_s_cosf += -D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_cosf
CC_OPT_s_cosf += -D__kernel_sindf=__kernel_sindf_cosf
CC_OPT_s_tanf += -D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_tanf

CC_OPT  += -D__inline=inline
INC_DIR += $(LIBC_DIR)/msun/src $(LIBC_DIR)/msun/ld80 $(LIBC_DIR)/msun/bsdsrc

# vi: set ft=make :
