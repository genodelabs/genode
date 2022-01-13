MIRROR_FROM_REP_DIR := \
	src/lib/vfs/gpu/vfs_gpu.h \
	lib/import/import-vfs_gpu.mk \
	lib/symbols/vfs_gpu

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

