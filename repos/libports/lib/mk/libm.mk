LIBC_PORT_DIR := $(call select_from_ports,libc)
LIBC_DIR      := $(LIBC_PORT_DIR)/src/lib/libc
LIBM_DIR      := $(LIBC_DIR)/lib/msun

LIBS = libc

#
# finding 'math_private.h'
#
INC_DIR += $(LIBM_DIR)/src

#
# finding 'invtrig.h', included by 'e_acosl.c'
#
INC_DIR += $(LIBM_DIR)/ld80

#
# finding 'fpmath.h', included by 'invtrig.h'
#
INC_DIR += $(LIBC_DIR)/lib/libc/include

FILTER_OUT += s_exp2l.c

#
# Files that are included by other sources (e.g., 's_sin.c'). Hence, we need
# to remove them from the build. Otherwise, we would end up with doubly
# defined symbols (and compiler warnings since those files are apparently
# not meant to be compiled individually).
#
FILTER_OUT += e_rem_pio2.c e_rem_pio2f.c

#
# Disable warnings for selected files, i.e., to suppress
# 'is static but used in inline function which is not static'
# messages
#
CC_OPT_s_tanf = -w
CC_OPT_s_tan  = -w
CC_OPT_s_sin  = -w
CC_OPT_s_cos  = -w
CC_OPT_s_cosf = -w
CC_OPT_s_sinf = -w
CC_OPT_k_cosf = -w
CC_OPT_k_sinf = -w
CC_OPT_k_tanf = -w

#
# Work-around to get over doubly defined symbols produced by several sources
# that include 'e_rem_pio2.c' and 'e_rem_pio2f.c'. To avoid symbol clashes,
# we rename each occurrence by adding the basename of the compilation unit
# as suffix.
#
CC_OPT_s_sin  += -D__ieee754_rem_pio2=__ieee754_rem_pio2_s_sin
CC_OPT_s_cos  += -D__ieee754_rem_pio2=__ieee754_rem_pio2_s_cos
CC_OPT_s_sinf += -D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_sinf
CC_OPT_s_sinf += -D__kernel_cosdf=__kernel_cosdf_sinf
CC_OPT_s_cosf += -D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_cosf
CC_OPT_s_cosf += -D__kernel_sindf=__kernel_sindf_cosf
CC_OPT_s_tanf += -D__ieee754_rem_pio2f=__ieee754_rem_pio2f_s_tanf

#
# Use default warning level rather than -Wall because we do not want to touch
# the imported source code to improve build aesthetics.
#
CC_WARN =

SRC_C  = $(wildcard $(LIBM_DIR)/src/*.c) \
         $(wildcard $(LIBM_DIR)/ld80/*.c) \
         $(wildcard $(LIBM_DIR)/bsdsrc/*.c)
SRC_C := $(filter-out $(FILTER_OUT),$(notdir $(SRC_C)))

# remove on update to version 9
SRC_C += log2.c

#
# 'e_rem_pio2.c' uses '__inline'
#
CC_OPT += -D__inline=inline

vpath %.c $(LIBM_DIR)/src
vpath %.c $(LIBM_DIR)/ld80
vpath %.c $(LIBM_DIR)/bsdsrc

# remove on update to version 9
vpath log2.c $(REP_DIR)/src/lib/libc

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
