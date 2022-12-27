PORT_DIR := $(call port_dir,$(REP_DIR)/ports/linux-firmware)

content: ucode_files LICENSE.wifi_drv


.PHONY: ucode_files
ucode_files:
	cp $(PORT_DIR)/firmware/*.ucode .
	cp $(PORT_DIR)/firmware/*.pnvm .
	cp $(PORT_DIR)/firmware/regulatory.db .
	cp $(PORT_DIR)/firmware/regulatory.db.p7s .

LICENSE.wifi_drv:
	for i in $(PORT_DIR)/firmware/LICEN*E.*; do \
	echo "$${i##*/}:" >> $@; \
	  cat $$i >> $@; \
	  echo >> $@; \
	done
