include $(GENODE_DIR)/repos/base/recipes/src/content.inc

content: src/drivers include/gpio

include/gpio:
	mkdir -p include
	cp -r $(REP_DIR)/include/gpio $@

src/drivers:
	mkdir -p $@/gpio/ $@/touch/ $@/sd_card
	cp -r $(REP_DIR)/src/drivers/gpio/imx/ $@/gpio
	cp -r $(REP_DIR)/src/drivers/touch/synaptics_dsx/ $@/touch
	cp -r $(REP_DIR)/src/drivers/sd_card/imx/ $@/sd_card/
	cp -r $(REP_DIR)/src/drivers/sd_card/imx6/ $@/sd_card/
	cp -r $(REP_DIR)/src/drivers/sd_card/imx8/ $@/sd_card/
	cp    $(REP_DIR)/src/drivers/sd_card/*.* $@/sd_card/
