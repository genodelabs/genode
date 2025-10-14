PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/dde_linux/ports/linux-firmware)

content: ucode_files LICENSE.wifi pc_wifi_firmware.tar


.PHONY: ucode_files
ucode_files:
	cp -R $(PORT_DIR)/firmware/rtlwifi .
	cp $(PORT_DIR)/firmware/*.ucode .
	cp $(PORT_DIR)/firmware/*.pnvm .
	cp $(PORT_DIR)/firmware/regulatory.db .
	cp $(PORT_DIR)/firmware/regulatory.db.p7s .

LICENSE.wifi:
	for i in $(PORT_DIR)/firmware/LICEN*E.*; do \
	echo "$${i##*/}:" >> $@; \
	  cat $$i >> $@; \
	  echo >> $@; \
	done

include $(GENODE_DIR)/repos/base/recipes/content.inc

pc_wifi_firmware.tar: ucode_files LICENSE.wifi
	$(TAR) --remove-files -cf $@ -C . *.* rtlwifi/*.* && rmdir rtlwifi
