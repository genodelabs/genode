TARGET = test-libc_fifo_pipe

LIBS := base
LIBS += libc
LIBS += vfs

SRC_CC := main.cc

CC_CXX_WARN_STRICT =
