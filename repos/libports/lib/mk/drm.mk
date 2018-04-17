include $(call select_from_repositories,lib/import/import-drm.mk)

SHARED_LIB   = yes
LIBS         = libc
DRM_SRC_DIR := $(call select_from_ports,drm)/src/lib/drm
LIB_DIR      = $(REP_DIR)/src/lib/drm

INC_DIR += $(LIB_DIR)/include

SRC_C = intel/intel_bufmgr.c \
        intel/intel_bufmgr_gem.c \
        intel/intel_decode.c \
        xf86drm.c xf86drmHash.c \
        xf86drmRandom.c

SRC_CC = dummies.cc ioctl.cc

CC_OPT = -DHAVE_LIBDRM_ATOMIC_PRIMITIVES=1 -DO_CLOEXEC=0

#
# We rename 'ioctl' calls to 'genode_ioctl' calls, this way we are not required
# to write a libc plugin for libdrm.
#
CC_C_OPT += -Dioctl=genode_ioctl

vpath %.c  $(DRM_SRC_DIR)
vpath %.cc $(LIB_DIR)

CC_CXX_WARN_STRICT =
