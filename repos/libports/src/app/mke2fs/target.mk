TARGET := mke2fs
LIBS   := posix e2fsprogs

E2FS_DIR := $(addsuffix /src/lib/e2fsprogs, $(call select_from_ports,e2fsprogs-lib))

INC_DIR += $(E2FS_DIR)/e2fsck

CC_DEF += -DROOT_SYSCONFDIR=\"/etc\"

SRC_C := $(addprefix misc/, mke2fs.c util.c default_profile.c)
SRC_C += $(addprefix e2fsck/, profile.c prof_err.c)

vpath %.c $(E2FS_DIR)

CC_CXX_WARN_STRICT =
