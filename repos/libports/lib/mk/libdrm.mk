SRC_C = intel_bufmgr_gem.c intel_bufmgr.c ioctl.cc

LIBDRM_DIR := $(call select_from_ports,libdrm)/src/lib/libdrm
INC_DIR    += $(LIBDRM_DIR) $(LIBDRM_DIR)/include/drm $(LIBDRM_DIR)/intel

LIBS   += libc
CC_OPT += -U__linux__
CC_OPT += -DHAVE_LIBDRM_ATOMIC_PRIMITIVES=1

vpath %.c      $(LIBDRM_DIR)/intel
vpath ioctl.cc $(REP_DIR)/src/lib/libdrm
