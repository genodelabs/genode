TARGET := e2fsck
LIBS   := posix e2fsprogs

E2FSCK_TOP_DIR := $(call select_from_ports,e2fsprogs-lib)
E2FSCK_DIR     := $(E2FSCK_TOP_DIR)/src/lib/e2fsprogs/e2fsck

INC_DIR += $(E2FSCK_DIR)/include/e2fsprogs

CC_DEF += -DHAVE_CONFIG_H
CC_DEF += -DROOT_SYSCONFDIR=\"/etc\"

CC_OPT += -Wno-unused-variable -Wno-parentheses

# e2fsck/dict.c
SRC_C := \
         badblocks.c \
         dirinfo.c \
         dx_dirinfo.c \
         e2fsck.c \
         ea_refcount.c \
         ehandler.c \
         encrypted_files.c \
         extents.c \
         journal.c \
         logfile.c \
         message.c \
         pass1.c \
         pass1b.c \
         pass2.c \
         pass3.c \
         pass4.c \
         pass5.c \
         problem.c \
         quota.c \
         recovery.c \
         readahead.c \
         region.c \
         rehash.c \
         revoke.c \
         sigcatcher.c \
         super.c \
         unix.c \
         util.c

INC_DIR += $(PRG_DIR)

vpath %.c $(E2FSCK_DIR)

CC_CXX_WARN_STRICT =
