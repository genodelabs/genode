LIBGPG_ERROR_DIR     := $(call select_from_ports,libgcrypt)/src/lib/libgpg-error
LIBGPG_ERROR_SRC_DIR := $(LIBGPG_ERROR_DIR)/src

LIBS := libc

SRC_C := visibility.c code-from-errno.c init.c estream.c estream-printf.c \
         strerror.c code-to-errno.c posix-lock.c posix-thread.c

INC_DIR += $(REP_DIR)/src/lib/libgpg-error $(LIBGPG_ERROR_SRC_DIR)

CC_OPT += -DHAVE_CONFIG_H

include $(REP_DIR)/lib/import/import-libgcrypt.mk

vpath %.c $(LIBGPG_ERROR_SRC_DIR)
