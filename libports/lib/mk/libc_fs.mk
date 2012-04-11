SRC_CC = plugin.cc
LIBS  += libc

vpath plugin.cc $(REP_DIR)/src/lib/libc_fs

SHARED_LIB = yes
