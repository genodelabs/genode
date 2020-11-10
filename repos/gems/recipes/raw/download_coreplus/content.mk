content: init.config assets.tar

init.config:
	cp $(REP_DIR)/recipes/raw/download_coreplus/$@ $@

assets.tar:
	tar --mtime='2018-05-29 00:00Z' -cf $@ -C $(REP_DIR)/recipes/raw/download_coreplus machine.vbox
	tar --mtime='2018-05-29 00:00Z' -rf $@ -C $(REP_DIR)/recipes/raw/download_coreplus disk0.vmdk
