SRC_DIR = src/server/cached_fs_rom
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: $(GENODE_DIR)/repos/os/include/file_system
	mkdir include
	cp -r $< include/file_system
