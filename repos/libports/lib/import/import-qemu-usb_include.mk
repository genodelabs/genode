QEMU_CONTRIB_DIR := $(call select_from_ports,qemu-usb)/src/lib/qemu

LIB_DIR     := $(REP_DIR)/src/lib/qemu-usb
LIB_INC_DIR := $(LIB_DIR)/include

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'qemu_emul.h' header.
INC_DIR += $(LIB_INC_DIR)
INC_DIR += $(QEMU_CONTRIB_DIR)/include
INC_DIR += $(LIB_CACHE_DIR)/qemu-usb_include/include
