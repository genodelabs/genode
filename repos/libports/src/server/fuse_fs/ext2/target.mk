include $(REP_DIR)/ports/fuse-ext2.inc
FUSE_EXT2_DIR = $(REP_DIR)/contrib/$(FUSE_EXT2)/fuse-ext2

TARGET   = ext2_fuse_fs

FILTER_OUT = fuse-ext2.probe.c fuse-ext2.wait.c
SRC_C  = $(filter-out $(FILTER_OUT), $(notdir $(wildcard $(FUSE_EXT2_DIR)/*.c)))


SRC_CC   = fuse_fs_main.cc \
           init.cc

LIBS     = base config server libc libfuse libext2fs

CC_OPT += -DHAVE_CONFIG_H -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

CC_OPT += -Wno-unused-function -Wno-unused-variable \
          -Wno-unused-but-set-variable -Wno-cpp \
          -Wno-implicit-function-declaration \
          -Wno-maybe-uninitialized


INC_DIR += $(REP_DIR)/src/lib/fuse-ext2 \
           $(FUSE_EXT2_DIR)

INC_DIR += $(PRG_DIR)/..

vpath %.c             $(FUSE_EXT2_DIR)
vpath fuse_fs_main.cc $(PRG_DIR)/..
vpath init.cc         $(PRG_DIR)/../../../lib/fuse-ext2
