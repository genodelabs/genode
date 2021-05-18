include $(REP_DIR)/lib/import/import-libuvc.mk

LIBUVC_DIR := $(call select_from_ports,libuvc)/src/lib/libuvc
LIBS       += libc libusb jpeg

INC_DIR += $(LIBUVC_DIR)/include

#CC_OPT += -DUVC_DEBUGGING

ALL_SRC_C := $(notdir $(wildcard $(LIBUVC_DIR)/src/*.c))
SRC_C := $(filter-out example.c test.c,$(ALL_SRC_C))

vpath %.c $(LIBUVC_DIR)/src

SHARED_LIB := yes

CC_CXX_WARN_STRICT =
