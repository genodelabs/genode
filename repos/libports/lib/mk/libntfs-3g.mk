include $(REP_DIR)/ports/ntfs-3g.inc
NTFS_3G_DIR = $(REP_DIR)/contrib/$(NTFS_3G)

FILTER_OUT = win32_io.c
SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(NTFS_3G_DIR)/libntfs-3g/*.c)))

INC_DIR += $(REP_DIR)/include/ntfs-3g \
           $(REP_DIR)/src/lib/ntfs-3g

CC_OPT += -DHAVE_CONFIG_H -DRECORD_LOCKING_NOT_IMPLEMENTED -DDEBUG

LIBS += libc

vpath %.c $(NTFS_3G_DIR)/libntfs-3g
