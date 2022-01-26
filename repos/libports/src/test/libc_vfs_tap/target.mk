TARGET = test-libc_vfs_tap

LIBS := base
LIBS += libc
LIBS += posix
LIBS += vfs

SRC_C := main.c
