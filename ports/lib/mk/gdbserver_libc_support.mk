INC_DIR += $(REP_DIR)/src/lib/gdbserver_libc_support

SRC_C += gdbserver_libc_support.c

LIBS += libc

vpath %.c  $(REP_DIR)/src/lib/gdbserver_libc_support
