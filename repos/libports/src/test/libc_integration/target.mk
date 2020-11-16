TARGET := test-libc_integration

LIBS   += libc
LIBS   += posix
LIBS   += stdcxx
LIBS   += vfs
LIBS   += vfs_pipe

SRC_CC := main.cc
SRC_CC += stdcxx_log.cc
SRC_CC += thread.cc
