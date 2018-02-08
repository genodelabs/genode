PORT_DIR := $(call port_dir,$(REP_DIR)/ports/fatfs)

MIRROR_FROM_REP_DIR := \
	lib/import/import-fatfs_block.mk \
	lib/mk/fatfs_block.mk

content: include src $(MIRROR_FROM_REP_DIR)

include:
	mkdir -p include/fatfs
	cp -r $(PORT_DIR)/$@/* $@
	cp $(REP_DIR)/include/fatfs/block.h include/fatfs

src:
	mkdir -p src/lib/fatfs
	cp -r $(PORT_DIR)/$@/* $@
	cp $(REP_DIR)/src/lib/fatfs/diskio_block.cc src/lib/fatfs/

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
