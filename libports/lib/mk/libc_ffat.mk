FFAT_DIR = $(REP_DIR)/contrib/ff007e

ifeq ($(wildcard $(FFAT_DIR)),)
REQUIRES += prepare_ffat
endif

SRC_CC = plugin.cc
LIBS  += libc ffat_block

vpath plugin.cc $(REP_DIR)/src/lib/libc_ffat

SHARED_LIB = yes
