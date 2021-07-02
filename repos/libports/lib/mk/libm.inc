LIBC_PORT_DIR := $(call select_from_ports,libc)
LIBC_DIR      := $(LIBC_PORT_DIR)/src/lib/libc
LIBM_DIR      := $(LIBC_DIR)/lib/msun

SHARED_LIB = yes

LIBS = libc

#
# finding 'math_private.h'
#
INC_DIR += $(LIBM_DIR)/src

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
# Use default warning level rather than -Wall because we do not want to touch
# the imported source code to improve build aesthetics.
#
CC_WARN =

SRC_C  = $(wildcard $(LIBM_DIR)/src/*.c) \
         $(wildcard $(LIBM_DIR)/bsdsrc/*.c)

#
# 128 bit quadruple precision format
#
ifeq ($(LIBM_LD128),yes)
INC_DIR += $(LIBM_DIR)/ld128
SRC_C   += $(wildcard $(LIBM_DIR)/ld128/*.c)

vpath %.c $(LIBM_DIR)/ld128
endif

#
# 80 bit extented precision format
#
ifeq ($(LIBM_LD80),yes)
INC_DIR += $(LIBM_DIR)/ld80
SRC_C   += $(wildcard $(LIBM_DIR)/ld80/*.c)

vpath %.c $(LIBM_DIR)/ld80
endif

SRC_C := $(filter-out $(FILTER_OUT),$(notdir $(SRC_C)))

vpath %.c $(LIBM_DIR)/src
vpath %.c $(LIBM_DIR)/bsdsrc

CC_CXX_WARN_STRICT =

# vi: set ft=make :
