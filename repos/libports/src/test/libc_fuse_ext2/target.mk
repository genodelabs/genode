TARGET   = test-libc_fuse_ext2
LIBS     = libc libc_fuse_ext2
SRC_CC   = main.cc

vpath %.cc $(REP_DIR)/src/test/libc_vfs

CC_CXX_WARN_STRICT =
