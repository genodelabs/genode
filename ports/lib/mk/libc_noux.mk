SRC_CC = plugin.cc

LIBS += libc

vpath %.cc $(REP_DIR)/src/lib/libc_noux

SHARED_LIB = yes
