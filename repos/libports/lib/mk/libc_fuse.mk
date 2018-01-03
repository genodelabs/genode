LIBS   = libc libfuse

SRC_CC = plugin.cc

vpath %.cc $(REP_DIR)/src/lib/libc_fuse

CC_CXX_WARN_STRICT =
