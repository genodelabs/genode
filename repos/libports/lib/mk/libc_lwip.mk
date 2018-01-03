SRC_CC = init.cc plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_lwip

LIBS += lwip libc

CC_CXX_WARN_STRICT =
