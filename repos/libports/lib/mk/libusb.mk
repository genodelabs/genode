LIBUSB_DIR := $(call select_from_ports,libusb)/src/lib/libusb
LIBS       += libc libc_pipe

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/libusb

INC_DIR += $(LIBUSB_DIR)/libusb

SRC_C = core.c \
        descriptor.c \
        io.c \
        strerror.c \
        sync.c \
        hotplug.c \
        os/poll_posix.c \
        os/threads_posix.c

SRC_CC = genode_usb_raw.cc

vpath %.c $(LIBUSB_DIR)/libusb
vpath %.cc $(REP_DIR)/src/lib/libusb

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
