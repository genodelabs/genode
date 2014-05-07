SRC_CC = plugin.cc
LIBS += libc

vpath %.cc $(REP_DIR)/src/lib/libc_lock_pipe

SHARED_LIB = yes
