SRC_DIR := src/app/decorator
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: include/decorator

include/decorator:
	mkdir -p $@
	cp $(GENODE_DIR)/repos/os/include/decorator/* $@

content: src/app/scout/data/droidsansb10.tff

src/app/scout/data/droidsansb10.tff:
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/demo/$@ $@
