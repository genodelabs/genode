content: drivers.config fb_drv.config event_filter.config en_us.chargen \
         special.chargen numlock_remap.config gpu_drv.config

drivers.config numlock_remap.config event_filter.config:
	cp $(REP_DIR)/recipes/raw/drivers_managed-pc/$@ $@

fb_drv.config:
	cp $(GENODE_DIR)/repos/os/recipes/raw/drivers_interactive-pc/$@ $@

gpu_drv.config:
	cp $(GENODE_DIR)/repos/os/src/drivers/gpu/intel/$@ $@

en_us.chargen special.chargen:
	cp $(GENODE_DIR)/repos/os/src/server/event_filter/$@ $@
