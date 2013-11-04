LIBS   = libc

SRC_CC = plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_block

include $(REP_DIR)/lib/mk/libc-common.inc

SHARED_LIB = yes
