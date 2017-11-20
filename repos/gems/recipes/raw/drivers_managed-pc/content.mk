content: drivers.config fb_drv.config input_filter.config en_us.chargen \
         numlock_remap.config

drivers.config numlock_remap.config:
	cp $(REP_DIR)/recipes/raw/drivers_managed-pc/$@ $@

fb_drv.config input_filter.config:
	cp $(GENODE_DIR)/repos/os/recipes/raw/drivers_interactive-pc/$@ $@

en_us.chargen:
	cp $(GENODE_DIR)/repos/os/src/server/input_filter/$@ $@
