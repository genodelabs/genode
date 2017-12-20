LIBARCHIVE_DIR = $(call select_from_ports,libarchive)/src/lib/libarchive
LIBS          += libc zlib liblzma
INC_DIR       += $(REP_DIR)/src/lib/libarchive $(LIBARCHIVE_DIR)

ALL_SRC_C := $(notdir $(wildcard $(LIBARCHIVE_DIR)/libarchive/*.c))

FILTER_OUT_SRC_C := archive_disk_acl_darwin.c \
                    archive_disk_acl_freebsd.c \
                    archive_disk_acl_linux.c \
                    archive_disk_acl_sunos.c \
                    archive_entry_copy_bhfi.c \
                    archive_read_disk_windows.c \
                    archive_windows.c \
                    archive_write_disk_windows.c \
                    filter_fork_posix.c


SRC_C := $(filter-out $(FILTER_OUT_SRC_C),$(ALL_SRC_C)) no_fork.c

vpath %.c       $(LIBARCHIVE_DIR)/libarchive
vpath no_fork.c $(REP_DIR)/src/lib/libarchive

CC_OPT += -DPLATFORM_CONFIG_H=\"genode_config.h\"

CC_WARN += -Wno-unused-variable -Wno-int-conversion

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
