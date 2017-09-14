SRC_CC = plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lxip

LIBS += lxip libc
