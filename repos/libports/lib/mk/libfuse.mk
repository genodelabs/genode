SRC_CC = fuse.cc

INC_DIR += $(REP_DIR)/include/fuse

LIBS = libc

CC_OPT += -fpermissive

vpath %.cc $(REP_DIR)/src/lib/fuse

CC_CXX_WARN_STRICT =
