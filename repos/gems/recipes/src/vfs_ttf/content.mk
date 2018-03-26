MIRROR_FROM_REP_DIR := lib/mk/vfs_ttf.mk lib/mk/ttf_font.mk \
                       src/lib/vfs/ttf src/lib/ttf_font

STB_PORT_DIR := $(call port_dir,$(GENODE_DIR)/repos/libports/ports/stb)

content: $(MIRROR_FROM_REP_DIR) include/stb_truetype.h LICENSE

include/stb_truetype.h:
	mkdir -p $(dir $@)
	cp -r $(STB_PORT_DIR)/include/stb_truetype.h $@

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
