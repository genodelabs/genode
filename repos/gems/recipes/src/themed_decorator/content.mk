SRC_DIR := src/app/themed_decorator
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/decorator

include/decorator:
	mkdir -p $@
	cp $(GENODE_DIR)/repos/os/include/decorator/* $@

content: src/lib/file lib/mk/file.mk

src/lib/file lib/mk/file.mk:
	$(mirror_from_rep_dir)
