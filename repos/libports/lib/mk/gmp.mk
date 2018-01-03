include $(REP_DIR)/lib/mk/gmp.inc

LIBS += gmp-mpn gmp-mpf gmp-mpz gmp-mpq

#
# Source codes from gmp base directory
#
SRC_C += assert.c compat.c errno.c extract-dbl.c invalid.c \
         memory.c mp_bpl.c mp_clz_tab.c mp_dv_tab.c \
         mp_minv_tab.c mp_get_fns.c mp_set_fns.c rand.c randclr.c \
         randdef.c randiset.c randlc2s.c randlc2x.c randmt.c \
         randmts.c rands.c randsd.c randsdui.c randbui.c randmui.c \
         version.c tal-reent.c

#
# Source codes from subdirectories
#
SRC_C += $(notdir $(wildcard $(GMP_DIR)/printf/*.c))
SRC_C += $(notdir $(wildcard $(GMP_DIR)/scanf/*.c))

vpath %.c $(GMP_DIR)
vpath %.c $(GMP_DIR)/printf
vpath %.c $(GMP_DIR)/scanf

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
