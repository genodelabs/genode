PORT_DIR := $(call port_dir,$(REP_DIR)/ports/ttf-bitstream-vera)

content:
	cp $(PORT_DIR)/ttf/bitstream-vera/COPYRIGHT.TXT LICENSE.bitstream-vera
	cp $(PORT_DIR)/ttf/bitstream-vera/Vera.ttf .
	cp $(PORT_DIR)/ttf/bitstream-vera/VeraMono.ttf .
