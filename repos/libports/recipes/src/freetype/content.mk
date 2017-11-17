content: include/freetype-genode src/lib/freetype lib/mk/freetype.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/freetype)

src/lib/freetype:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/freetype $@
	echo "LIBS = freetype" > $@/target.mk

include/freetype-genode:
	$(mirror_from_rep_dir)

lib/mk/%.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/freetype/contrib/docs/LICENSE.TXT $@
