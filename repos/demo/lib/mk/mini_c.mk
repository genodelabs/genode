SRC_CC = snprintf.cc vsnprintf.cc atol.cc strtol.cc strtod.cc \
         malloc_free.cc memcmp.cc strlen.cc memset.cc abort.cc \
         mini_c.cc

STDINC = yes

vpath % $(REP_DIR)/src/lib/mini_c

include $(REP_DIR)/lib/import/import-mini_c.mk
