content: drivers.config fb_drv.config event_filter.config en_us.chargen special.chargen

drivers.config fb_drv.config event_filter.config:
	cp $(REP_DIR)/recipes/raw/drivers_interactive-imx8q_evk/$@ $@

en_us.chargen special.chargen:
	cp $(GENODE_DIR)/repos/os/src/server/event_filter/$@ $@
