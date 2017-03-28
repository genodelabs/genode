include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/drivers include

src/drivers:
	mkdir -p $@/framebuffer $@/input/ps2
	cp -r $(REP_DIR)/src/drivers/framebuffer/spec/pl11x/* $@/framebuffer
	cp    $(REP_DIR)/src/drivers/input/spec/ps2/*.h       $@/input/ps2/
	cp -r $(REP_DIR)/src/drivers/input/spec/ps2/pl050     $@/input/ps2/
	sed -i "/REQUIRES/s/=.*/= arm/" src/drivers/framebuffer/pbxa9/target.mk
	sed -i "/REQUIRES/s/=.*/= arm/" src/drivers/input/ps2/pl050/target.mk

include:
	mkdir -p $@
	cp    $(REP_DIR)/include/spec/pbxa9/*.h $@
	cp -r $(GENODE_DIR)/repos/base/include/spec/pbxa9/drivers $@
