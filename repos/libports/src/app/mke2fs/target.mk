TARGET := mke2fs
LIBS   := posix e2fsprogs

E2FS_DIR := $(addsuffix /src/lib/e2fsprogs, $(call select_from_ports,e2fsprogs-lib))

INC_DIR += $(E2FS_DIR)/e2fsck

CC_DEF += -DROOT_SYSCONFDIR=\"/etc\"

SRC_C := \
         create_inode.c \
         default_profile.c \
         mk_hugefiles.c \
         mke2fs.c \
         util.c

vpath %.c $(E2FS_DIR)/misc

CC_CXX_WARN_STRICT =
