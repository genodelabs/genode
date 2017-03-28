#
# Gallium driver for i915, loaded as needed at runtime (via 'dlopen')
#

REQUIRES += i915

include $(REP_DIR)/lib/mk/gallium.inc

# i915 driver
SRC_C := $(notdir $(wildcard $(GALLIUM_SRC_DIR)/drivers/i915/*.c))
vpath %.c $(GALLIUM_SRC_DIR)/drivers/i915

# dummy stub for trace driver
SRC_C += dummy_trace.c
vpath dummy_trace.c $(REP_DIR)/src/lib/gallium
INC_DIR += $(GALLIUM_SRC_DIR)/drivers/trace

SRC_CC += query_device_id.cc
vpath query_device_id.cc $(REP_DIR)/src/lib/gallium/i915

# libdrm includes
LIBDRM_DIR := $(call select_from_ports,libdrm)/src/lib/libdrm
INC_DIR += $(LIBDRM_DIR)/include/drm $(LIBDRM_DIR)/intel

# interface to i915 drm device
SRC_C += $(notdir $(wildcard $(GALLIUM_SRC_DIR)/winsys/drm/intel/gem/*.c))
vpath %.c $(GALLIUM_SRC_DIR)/winsys/drm/intel/gem

LIBS += libdrm
LIBS += gpu_i915_drv

SHARED_LIB = yes
