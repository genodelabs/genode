SRC_DIR := src/app/themed_decorator
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/decorator

include/decorator:
	mkdir -p $@
	cp $(GENODE_DIR)/repos/os/include/decorator/* $@
