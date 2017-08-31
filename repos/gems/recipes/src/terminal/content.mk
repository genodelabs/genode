SRC_DIR := src/server/terminal
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/terminal

include/terminal:
	mkdir -p $@
	cp $(GENODE_DIR)/repos/os/include/terminal/* $@
