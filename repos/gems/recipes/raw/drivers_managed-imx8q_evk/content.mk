content: drivers.config fb_drv.config event_filter.config en_us.chargen \
         special.chargen numlock_remap.config block_devices.report

drivers.config numlock_remap.config event_filter.config block_devices.report:
	cp $(REP_DIR)/recipes/raw/drivers_managed-imx8q_evk/$@ $@

fb_drv.config:
	cp $(GENODE_DIR)/repos/dde_linux/recipes/raw/drivers_interactive-imx8q_evk/$@ $@

en_us.chargen special.chargen:
	cp $(GENODE_DIR)/repos/os/src/server/event_filter/$@ $@
