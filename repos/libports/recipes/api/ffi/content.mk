MIRROR_FROM_REP_DIR  := lib/symbols/ffi

content: LICENSE include $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/ffi)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/ffi/* $@

LICENSE:
	cp $(PORT_DIR)/src/lib/ffi/LICENSE $@
