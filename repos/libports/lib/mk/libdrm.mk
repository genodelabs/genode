include $(REP_DIR)/lib/mk/libdrm.inc

include $(call select_from_repositories,lib/import/import-libdrm.mk)

SRC_CC := ioctl_dummy.cc

CC_OPT_xf86drm += -Wno-unused-variable
