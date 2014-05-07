
include $(REP_DIR)/lib/mk/rump.inc

LIBS    += rump

RUMP_LIBS = librumpdev.a         \
            librumpdev_disk.a    \
            librumpdev_netsmb.a  \
            librumpkern_crypto.a \
            librumpnet.a         \
            librumpvfs.a         \
            librumpfs_cd9660.a   \
            librumpfs_ext2fs.a   \
            librumpfs_ffs.a      \
            librumpfs_msdos.a    \
            librumpfs_nfs.a      \
            librumpfs_ntfs.a     \
            librumpfs_smbfs.a    \
            librumpfs_udf.a

ARCHIVE += $(addprefix $(RUMP_LIB)/,$(RUMP_LIBS))


