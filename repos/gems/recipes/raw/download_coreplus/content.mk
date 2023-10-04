content: init.config assets.tar

init.config:
	cp $(REP_DIR)/recipes/raw/download_coreplus/$@ $@

TAR_OPT := --owner=0 --group=0 --numeric-owner --mode='go=' --mtime='2018-05-29 00:00Z'

assets.tar:
	tar $(TAR_OPT) -cf $@ -C $(REP_DIR)/recipes/raw/download_coreplus machine.vbox
	tar $(TAR_OPT) -rf $@ -C $(REP_DIR)/recipes/raw/download_coreplus disk0.vmdk
