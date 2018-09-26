SRC_DIR := src/app/window_layouter
include $(GENODE_DIR)/repos/base/recipes/src/content.inc

DECORATOR_INCLUDES := $(addprefix include/decorator/,xml_utils.h types.h)

content: $(DECORATOR_INCLUDES)

$(DECORATOR_INCLUDES):
	mkdir -p $(dir $@)
	cp $(GENODE_DIR)/repos/os/$@ $@
