include $(REP_DIR)/lib/mk/libdrm.inc

include $(call select_from_repositories,lib/import/import-libdrm-iris.mk)

SRC_CC := ioctl_iris.cc
