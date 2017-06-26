include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/drivers

src/drivers:
	mkdir -p $@/framebuffer $@/input/ps2
	cp -r $(REP_DIR)/src/drivers/framebuffer/spec/pl11x/* $@/framebuffer
	cp    $(REP_DIR)/src/drivers/input/spec/ps2/*.h       $@/input/ps2/
	cp -r $(REP_DIR)/src/drivers/input/spec/ps2/pbxa9     $@/input/ps2/
	sed -i "/REQUIRES/s/=.*/= arm/" src/drivers/framebuffer/pbxa9/target.mk
	sed -i "/REQUIRES/s/=.*/= arm/" src/drivers/input/ps2/pbxa9/target.mk
