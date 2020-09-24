include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/drivers include/gpio

include/gpio:
	mkdir -p include
	cp -r $(REP_DIR)/include/gpio $@

src/drivers:
	mkdir -p $@/gpio/ $@/touch/
	cp -r $(REP_DIR)/src/drivers/gpio/imx/ $@/gpio
	cp -r $(REP_DIR)/src/drivers/touch/synaptics_dsx/ $@/touch
