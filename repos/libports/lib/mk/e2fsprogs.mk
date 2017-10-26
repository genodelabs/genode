E2FSPROGS_DIR := $(call select_from_ports,e2fsprogs-lib)/src/lib/e2fsprogs

INC_DIR += $(call select_from_ports,e2fsprogs-lib)/include/e2fsprogs

LIBS := libc e2fsprogs_host_tools

CC_OPT += -Wno-unused-variable -Wno-unused-function -Wno-maybe-uninitialized

CC_DEF += -DLOCALEDIR=\"/share/locale\"
CC_DEF += -DLIBDIR=\"/lib\"
CC_DEF += -DLOCALE_ALIAS_PATH=\"/share/locale\"

SRC_C_intl := \
              intl/bindtextdom.c \
              intl/dcgettext.c \
              intl/dgettext.c \
              intl/gettext.c \
              intl/finddomain.c \
              intl/loadmsgcat.c \
              intl/localealias.c \
              intl/textdomain.c \
              intl/l10nflist.c \
              intl/dcigettext.c \
              intl/explodename.c \
              intl/dcngettext.c \
              intl/dngettext.c \
              intl/ngettext.c \
              intl/plural.c \
              intl/plural-exp.c \
              intl/localcharset.c \
              intl/relocatable.c \
              intl/log.c \
              intl/localename.c \
              intl/printf.c \
              intl/osdep.c \
              intl/intl-compat.c
INC_DIR_intl := $(E2FSPROGS_DIR)/intl $(REP_DIR)/src/lib/e2fsprogs/intl
CC_OPT_intl/dcigettext += -DSTATIC=

SRC_C_libblkid := \
                  lib/blkid/cache.c \
                  lib/blkid/devname.c \
                  lib/blkid/dev.c \
                  lib/blkid/getsize.c \
                  lib/blkid/devno.c \
                  lib/blkid/llseek.c \
                  lib/blkid/probe.c \
                  lib/blkid/read.c \
                  lib/blkid/resolve.c \
                  lib/blkid/save.c \
                  lib/blkid/tag.c \
                  lib/blkid/version.c
INC_DIR_libblkid := $(E2FSPROGS_DIR)/lib/blkid $(REP_DIR)/src/lib/e2fsprogs/lib/blkid

SRC_C_libcom_err := \
                    lib/et/com_err.c \
                    lib/et/com_right.c \
                    lib/et/error_message.c \
                    lib/et/et_name.c \
                    lib/et/init_et.c
INC_DIR_libcom_err := $(E2FSPROGS_DIR)/lib/et

SRC_C_libe2p := \
                lib/e2p/feature.c \
                lib/e2p/fgetflags.c \
                lib/e2p/fgetversion.c \
                lib/e2p/fsetflags.c \
                lib/e2p/fsetversion.c \
                lib/e2p/getflags.c \
                lib/e2p/getversion.c \
                lib/e2p/hashstr.c \
                lib/e2p/iod.c \
                lib/e2p/ls.c \
                lib/e2p/mntopts.c \
                lib/e2p/ostype.c \
                lib/e2p/parse_num.c \
                lib/e2p/pe.c \
                lib/e2p/percent.c \
                lib/e2p/pf.c \
                lib/e2p/ps.c \
                lib/e2p/setflags.c \
                lib/e2p/setversion.c \
                lib/e2p/uuid.c
INC_DIR_libe2p := $(E2FSPROGS_DIR)/lib/e2p

SRC_C_libext2fs := \
                   lib/ext2fs/alloc.c \
                   lib/ext2fs/alloc_sb.c \
                   lib/ext2fs/alloc_stats.c \
                   lib/ext2fs/alloc_tables.c \
                   lib/ext2fs/badblocks.c \
                   lib/ext2fs/bb_compat.c \
                   lib/ext2fs/bb_inode.c \
                   lib/ext2fs/bitmaps.c \
                   lib/ext2fs/bitops.c \
                   lib/ext2fs/blkmap64_ba.c \
                   lib/ext2fs/blkmap64_rb.c \
                   lib/ext2fs/blknum.c \
                   lib/ext2fs/block.c \
                   lib/ext2fs/bmap.c \
                   lib/ext2fs/check_desc.c \
                   lib/ext2fs/closefs.c \
                   lib/ext2fs/crc16.c \
                   lib/ext2fs/crc32c.c \
                   lib/ext2fs/csum.c \
                   lib/ext2fs/dblist.c \
                   lib/ext2fs/dblist_dir.c \
                   lib/ext2fs/dir_iterate.c \
                   lib/ext2fs/dirblock.c \
                   lib/ext2fs/dirhash.c \
                   lib/ext2fs/dupfs.c \
                   lib/ext2fs/expanddir.c \
                   lib/ext2fs/ext2_err.c \
                   lib/ext2fs/ext_attr.c \
                   lib/ext2fs/extent.c \
                   lib/ext2fs/fileio.c \
                   lib/ext2fs/finddev.c \
                   lib/ext2fs/flushb.c \
                   lib/ext2fs/freefs.c \
                   lib/ext2fs/gen_bitmap.c \
                   lib/ext2fs/gen_bitmap64.c \
                   lib/ext2fs/get_pathname.c \
                   lib/ext2fs/getsectsize.c \
                   lib/ext2fs/getsize.c \
                   lib/ext2fs/i_block.c \
                   lib/ext2fs/icount.c \
                   lib/ext2fs/imager.c \
                   lib/ext2fs/ind_block.c \
                   lib/ext2fs/initialize.c \
                   lib/ext2fs/inline.c \
                   lib/ext2fs/inode.c \
                   lib/ext2fs/inode_io.c \
                   lib/ext2fs/io_manager.c \
                   lib/ext2fs/ismounted.c \
                   lib/ext2fs/link.c \
                   lib/ext2fs/llseek.c \
                   lib/ext2fs/lookup.c \
                   lib/ext2fs/mkdir.c \
                   lib/ext2fs/mkjournal.c \
                   lib/ext2fs/mmp.c \
                   lib/ext2fs/namei.c \
                   lib/ext2fs/native.c \
                   lib/ext2fs/newdir.c \
                   lib/ext2fs/openfs.c \
                   lib/ext2fs/progress.c \
                   lib/ext2fs/punch.c \
                   lib/ext2fs/qcow2.c \
                   lib/ext2fs/rbtree.c \
                   lib/ext2fs/read_bb.c \
                   lib/ext2fs/read_bb_file.c \
                   lib/ext2fs/res_gdt.c \
                   lib/ext2fs/rw_bitmaps.c \
                   lib/ext2fs/swapfs.c \
                   lib/ext2fs/symlink.c \
                   lib/ext2fs/tdb.c \
                   lib/ext2fs/test_io.c \
                   lib/ext2fs/undo_io.c \
                   lib/ext2fs/unix_io.c \
                   lib/ext2fs/unlink.c \
                   lib/ext2fs/valid_blk.c \
                   lib/ext2fs/version.c \
                   lib/ext2fs/write_bb_file.c
INC_DIR_libext2fs := $(E2FSPROGS_DIR)/lib/ext2fs $(REP_DIR)/src/lib/e2fsprogs/lib/ext2fs

SRC_C_libquota := \
                  lib/quota/mkquota.c \
                  lib/quota/quotaio.c \
                  lib/quota/quotaio_v2.c \
                  lib/quota/quotaio_tree.c \
                  e2fsck/dict.c
INC_DIR_libquota := $(E2FSPROGS_DIR)/lib/quota

SRC_C_libuuid := \
                 lib/uuid/clear.c \
                 lib/uuid/compare.c \
                 lib/uuid/copy.c \
                 lib/uuid/gen_uuid.c \
                 lib/uuid/isnull.c \
                 lib/uuid/pack.c \
                 lib/uuid/parse.c \
                 lib/uuid/unpack.c \
                 lib/uuid/unparse.c \
                 lib/uuid/uuid_time.c
INC_DIR_libuuid := $(E2FSPROGS_DIR)/lib/uuid

SRC_C := \
         $(SRC_C_intl) \
         $(SRC_C_libblkid) \
         $(SRC_C_libcom_err) \
         $(SRC_C_libe2p) \
         $(SRC_C_libext2fs) \
         $(SRC_C_libquota) \
         $(SRC_C_libuuid)

INC_DIR += $(E2FSPROGS_DIR)/lib
INC_DIR += $(REP_DIR)/src/lib/e2fsprogs
INC_DIR += $(REP_DIR)/src/lib/e2fsprogs/lib
INC_DIR += $(INC_DIR_intl)
INC_DIR += $(INC_DIR_libblkid)
INC_DIR += $(INC_DIR_libcom_err)
INC_DIR += $(INC_DIR_libe2p)
INC_DIR += $(INC_DIR_libext2fs)
INC_DIR += $(INC_DIR_libquota)
INC_DIR += $(INC_DIR_libuuid)


CC_DEF += -D__BSD_VISIBLE
CC_DEF += -DHAVE_CONFIG_H

vpath %.c $(E2FSPROGS_DIR)

#
# Generate header files
#

# use top directory so that $(INC_DIR) picks it up
E2FSPROGS_BUILD_DIR := $(BUILD_BASE_DIR)/var/libcache/e2fsprogs
CRC_HEADER          := $(E2FSPROGS_BUILD_DIR)/crc32c_table.h

$(SRC_C:.c=.o): \
	$(CRC_HEADER)

EXT2FS_GEN_CRC := $(BUILD_BASE_DIR)/tool/e2fsprogs/gen_crc32ctable

$(CRC_HEADER):
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(EXT2FS_GEN_CRC) > $@
