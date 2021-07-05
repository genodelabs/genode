include $(REP_DIR)/lib/mk/libdrm.inc

include $(call select_from_repositories,lib/import/import-libdrm.mk)

#
# Enable when GPU multiplexer is available for Vivante
#
#SRC_CC := ioctl_etnaviv.cc
