content: drivers.config fb_drv.config event_filter.config en_us.chargen \
         special.chargen numlock_remap.config gpu_drv.config

drivers.config:
	cp $(REP_DIR)/sculpt/drivers/pc $@

numlock_remap.config:
	cp $(REP_DIR)/sculpt/numlock_remap/default $@

event_filter.config:
	cp $(REP_DIR)/sculpt/event_filter/pc $@

fb_drv.config:
	cp $(REP_DIR)/sculpt/fb_drv/default $@

gpu_drv.config:
	cp $(REP_DIR)/sculpt/gpu_drv/intel $@

en_us.chargen special.chargen:
	cp $(GENODE_DIR)/repos/os/src/server/event_filter/$@ $@
