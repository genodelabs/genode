include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/drivers

src/drivers:
	mkdir -p $@/framebuffer $@/input/ps2
	cp    -r $(REP_DIR)/src/drivers/framebuffer/pl11x/*  $@/framebuffer
	cp       $(REP_DIR)/src/drivers/input/spec/ps2/*.h   $@/input/ps2/
	cp    -r $(REP_DIR)/src/drivers/input/spec/ps2/pl050 $@/input/ps2/
