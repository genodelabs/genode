SRC_CC = vfs.cc

INC_DIR += $(REP_DIR)/src/lib/vfs/jitterentropy

LIBS  += libc jitterentropy

vpath %.cc $(REP_DIR)/src/lib/vfs/jitterentropy

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
