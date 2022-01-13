include $(REP_DIR)/lib/mk/libdrm.inc

include $(call select_from_repositories,lib/import/import-libdrm.mk)

LIBS += vfs_gpu

SRC_CC := ioctl_iris.cc

CC_CXX_WARN_STRICT =
