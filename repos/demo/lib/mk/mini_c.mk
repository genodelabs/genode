SRC_C  = mini_c.c
SRC_CC = snprintf.cc vsnprintf.cc atol.cc strtol.cc strtod.cc \
         malloc_free.cc memcmp.cc strlen.cc memset.cc abort.cc \
         printf.cc

STDINC = yes

vpath % $(REP_DIR)/src/lib/mini_c

include $(REP_DIR)/lib/import/import-mini_c.mk
