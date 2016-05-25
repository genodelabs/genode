REQUIRES = conversion_to_vfs_plugin

NTFS_3G_DIR := $(call select_from_ports,ntfs-3g)/src/lib/ntfs-3g

SRC_C  = ntfs-3g.c ntfs-3g_common.c
SRC_CC = init.cc

LIBS   = libc libc_fuse libfuse libntfs-3g

CC_OPT = -DHAVE_TIMESPEC -DHAVE_CONFIG_H -DRECORD_LOCKING_NOT_IMPLEMENTED

INC_DIR += $(REP_DIR)/src/lib/ntfs-3g \
           $(NTFS_3G_DIR)/src

vpath %.c  $(NTFS_3G_DIR)/src
vpath %.cc $(REP_DIR)/src/lib/ntfs-3g

SHARED_LIB = yes
