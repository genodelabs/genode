content: lib/symbols/libsanitizer_common \
         lib/symbols/libubsan \
         LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sanitizer)

lib/symbols/%:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/sanitizer/LICENSE.TXT $@

