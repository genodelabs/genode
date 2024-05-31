include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/drivers

src/drivers:
	mkdir -p $@/framebuffer
	cp    -r $(REP_DIR)/src/driver/framebuffer/ram/*  $@/framebuffer
