#
# Requires ld80/ld128 not supported on arm
#
FILTER_OUT = catrigl.c e_atan2l.c e_coshl.c e_sinhl.c \
             s_atanl.c s_fmal.c s_tanhl.c

#
# Unsupported long double format
#
FILTER_OUT += e_acosl.c e_asinl.c e_acoshl.c e_atanhl.c \
              s_asinhl.c s_clogl.c s_cosl.c s_csqrtl.c \
              s_frexpl.c s_nextafterl.c s_nexttoward.c \
              s_rintl.c s_scalbnl.c s_sincosl.c s_sinl.c \
              s_tanl.c

#
# Unsupported 'xbits' in 'IEEEl2bits'
#
FILTER_OUT += e_hypotl.c s_cbrtl.c s_roundl.c

include $(REP_DIR)/lib/mk/libm.inc

SRC_C += msun/arm/fenv.c

vpath msun/arm/fenv.c $(LIBC_DIR)/lib
