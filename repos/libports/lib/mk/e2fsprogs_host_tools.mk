#
# Compile host tools used to create generated header files
#

EXT2FS_GEN_CRC := $(BUILD_BASE_DIR)/tool/e2fsprogs/gen_crc32ctable
E2FSCK_GEN_CRC := $(BUILD_BASE_DIR)/tool/e2fsprogs/gen_crc32table

HOST_TOOLS += $(EXT2FS_GEN_CRC) $(E2FSCK_GEN_CRC)

E2FSPROGS_DIR := $(call select_from_ports,e2fsprogs-lib)/src/lib/e2fsprogs

$(EXT2FS_GEN_CRC): $(E2FSPROGS_DIR)
	$(MSG_BUILD)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)gcc $(E2FSPROGS_DIR)/lib/ext2fs/$(notdir $@).c -o $@

$(E2FSCK_GEN_CRC): $(E2FSPROGS_DIR)
	$(MSG_BUILD)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)gcc $(E2FSPROGS_DIR)/e2fsck/$(notdir $@).c -o $@

CC_CXX_WARN_STRICT =
