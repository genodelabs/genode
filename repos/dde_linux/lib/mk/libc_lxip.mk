SRC_CC = plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lxip

LIBS += lxip libc

CC_CXX_WARN_STRICT =
