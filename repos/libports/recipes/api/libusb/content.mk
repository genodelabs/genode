content: include lib/symbols/libusb LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libusb)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/libusb/* $@/

lib/symbols/libusb:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libusb/COPYING $@

