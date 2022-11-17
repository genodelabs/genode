content: ntp_rtc.config

ntp_rtc.config:
	cp $(REP_DIR)/recipes/raw/sntp_dummy_rtc/$@ $@
