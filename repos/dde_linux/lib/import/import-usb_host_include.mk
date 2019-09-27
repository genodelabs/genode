USB_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/usb_host

include $(call select_from_repositories,lib/import/import-usb_arch_include.mk)

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(ARCH_SRC_INC_DIR)
INC_DIR += $(USB_CONTRIB_DIR)/include
INC_DIR += $(USB_CONTRIB_DIR)/include/uapi
INC_DIR += $(USB_CONTRIB_DIR)/drivers/usb/core
INC_DIR += $(LIB_CACHE_DIR)/usb_host_include/include/include/include
