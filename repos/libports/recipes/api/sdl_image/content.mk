MIRROR_FROM_REP_DIR := lib/symbols/sdl_image lib/import/import-sdl_image.mk

content: $(MIRROR_FROM_REP_DIR) include LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR  := $(call port_dir,$(REP_DIR)/ports/sdl_image)

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/SDL $@/

LICENSE:
	cp $(PORT_DIR)/src/lib/sdl_image/COPYING $@
