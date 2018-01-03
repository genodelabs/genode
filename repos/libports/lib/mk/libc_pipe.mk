SRC_CC = plugin.cc
LIBS += libc

vpath %.cc $(REP_DIR)/src/lib/libc_pipe

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
