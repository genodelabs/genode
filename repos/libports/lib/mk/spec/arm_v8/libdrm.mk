include $(REP_DIR)/lib/mk/libdrm.inc

LIBS += vfs_gpu

include $(call select_from_repositories,lib/import/import-libdrm.mk)
include $(call select_from_repositories,lib/import/import-mesa_api.mk)

SRC_CC := ioctl_dispatch.cc ioctl_etnaviv.cc ioctl_lima.cc
