SRC_CC = plugin.cc
LIBS  += libc

vpath plugin.cc $(REP_DIR)/src/lib/libc_terminal

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
