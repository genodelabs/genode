MPFR_PORT_DIR := $(call select_from_ports,mpfr)

LIBS      = libc gmp

CC_OPT   += -DHAVE_STDARG -DHAVE_VA_COPY -DHAVE_INTTYPES_H -DHAVE_LITTLE_ENDIAN

INC_DIR  += $(REP_DIR)/include/mpfr \
            $(MPFR_PORT_DIR)/include/mpfr \
            $(MPFR_PORT_DIR)/include

MPFR_SRC_C := $(notdir $(wildcard $(MPFR_PORT_DIR)/src/lib/mpfr/src/*.c))
FILTER_OUT := jyn_asympt.c round_raw_generic.c
SRC_C      := $(filter-out $(FILTER_OUT),$(MPFR_SRC_C))

vpath %.c $(MPFR_PORT_DIR)/src/lib/mpfr/src

SHARED_LIB = 1

CC_CXX_WARN_STRICT =
