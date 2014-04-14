TARGET   = test-libc_fuse_exfat
LIBS     = libc libc_vfs libc_fuse_exfat
SRC_CC   = main.cc

vpath %.cc $(PRG_DIR)/../libc_ffat/
