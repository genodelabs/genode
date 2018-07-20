
include $(REP_DIR)/lib/import/import-zircon.mk

SHARED_LIB = yes

LIBS += base

SRC_C = stdio.c\
	printf.c

SRC_CC = libc.cc \
	 mutex.cc \
	 threads.cc \
	 device.cc \
	 syscalls.cc \
	 io_port.cc \
	 irq.cc \
	 debug.cc

vpath %.cc $(LIB_DIR)
vpath %.c $(ZIRCON_KERNEL)/lib/libc
