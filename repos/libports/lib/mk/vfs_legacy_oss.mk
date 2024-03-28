SRC_CC := vfs_oss.cc

vpath %.cc $(REP_DIR)/src/lib/vfs/legacy_oss

LIBS := libc

SHARED_LIB := yes

CC_CXX_WARN_STRICT_CONVERSION =
