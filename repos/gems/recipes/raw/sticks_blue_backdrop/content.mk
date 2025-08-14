PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sticks_blue_backdrop)

content: backdrop.config

backdrop.config:
	cp $(REP_DIR)/recipes/raw/sticks_blue_backdrop/$@ $@

content: sticks_blue.png

sticks_blue.png:
	cp $(PORT_DIR)/$@ $@
