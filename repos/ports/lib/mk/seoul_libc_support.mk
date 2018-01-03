include $(call select_from_repositories,lib/mk/libc-common.inc)

SRC_C  = stdlib/strtoul.c
SRC_C += $(addprefix string/,strchr.c strncpy.c strspn.c strcspn.c strstr.c strlen.c strnlen.c strcpy.c memcmp.c strcmp.c)
SRC_C += sys/__error.c gen/errno.c locale/none.c locale/table.c

vpath %.c $(LIBC_DIR)/lib/libc

CC_CXX_WARN_STRICT =
