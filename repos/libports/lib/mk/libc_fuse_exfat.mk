REQUIRES = conversion_to_vfs_plugin

EXFAT_DIR := $(call select_from_ports,exfat)/src/lib/exfat

SRC_C  = $(notdir $(EXFAT_DIR)/fuse/main.c)
SRC_CC = init.cc

LIBS   = libc libc_fuse libfuse libexfat

vpath %.c $(EXFAT_DIR)/fuse
vpath %.cc $(REP_DIR)/src/lib/exfat

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
