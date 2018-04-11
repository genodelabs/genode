content: drivers.config input_filter.config en_us.chargen special.chargen

drivers.config input_filter.config:
	cp $(REP_DIR)/recipes/raw/drivers_interactive-pbxa9/$@ $@

en_us.chargen special.chargen:
	cp $(REP_DIR)/src/server/input_filter/$@ $@
