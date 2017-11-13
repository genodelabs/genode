PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_linux)

content: ucode_files LICENSE.iwlwifi


.PHONY: ucode_files
ucode_files:
	cp $(PORT_DIR)/firmware/*.ucode .

LICENSE.iwlwifi:
	for i in $(PORT_DIR)/firmware/LICENSE.*; do \
	echo "$${i##*/}:" >> $@; \
	  cat $$i >> $@; \
	  echo >> $@; \
	done
