MIRROR_FROM_REP_DIR := src/app/resize2fs src/app/e2fsck src/app/mke2fs \
                       lib/mk/e2fsprogs.mk lib/import/import-e2fsprogs.mk \
                       lib/mk/e2fsprogs_host_tools.mk

content: src/lib/e2fsprogs include $(MIRROR_FROM_REP_DIR) LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/e2fsprogs-lib)

include:
	mkdir -p $@
	cp -a $(PORT_DIR)/include/* $@

src/lib/e2fsprogs:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/lib/e2fsprogs/* $@
	cp -a $(REP_DIR)/src/lib/e2fsprogs/* $@

LICENSE:
	cp $(PORT_DIR)/src/lib/e2fsprogs/COPYING $@

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
