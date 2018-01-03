#
# Cannot be compiled for ARM because this file relies on the 'xbits'
# member of union 'IEEEl2bits', which is not present in the ARM version.
#
FILTER_OUT = e_acosl.c e_asinl.c e_atan2l.c e_hypotl.c \
             s_atanl.c

#
# Cannot be compiled for ARM because "Unsupported long double format"
#
FILTER_OUT += s_cosl.c s_frexpl.c s_nextafterl.c s_nexttoward.c \
              s_rintl.c s_scalbnl.c s_sinl.c s_tanl.c

#
# Cannot be compiled for ARM because 'split' is undeclared
#
FILTER_OUT += s_fmal.c

include $(REP_DIR)/lib/mk/libm.mk

SRC_C  += msun/arm/fenv.c

vpath msun/arm/fenv.c $(LIBC_DIR)/lib

CC_CXX_WARN_STRICT =
