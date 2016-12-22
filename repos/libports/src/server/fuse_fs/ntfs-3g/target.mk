NTFS_3G_DIR := $(call select_from_ports,ntfs-3g)/src/lib/ntfs-3g

TARGET  := ntfs-3g_fuse_fs

SRC_C   := ntfs-3g.c ntfs-3g_common.c
SRC_CC  := fuse_fs_main.cc \
           init.cc

LIBS    := config libc libfuse libntfs-3g

CC_OPT  := -DHAVE_TIMESPEC -DHAVE_CONFIG_H -DRECORD_LOCKING_NOT_IMPLEMENTED

INC_DIR += $(PRG_DIR)/..

INC_DIR += $(REP_DIR)/src/lib/ntfs-3g \
           $(NTFS_3G_DIR)/src \
           $(REP_DIR)/contrib/$(NTFS_3G)/src

vpath %.c             $(NTFS_3G_DIR)/src
vpath fuse_fs_main.cc $(PRG_DIR)/..
vpath %.cc            $(REP_DIR)/src/lib/ntfs-3g
