content: src/lib/openjpeg lib/mk/openjpeg.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openjpeg)

src/lib/openjpeg:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/openjpeg $@
	echo "LIBS = openjpeg" > $@/target.mk

lib/mk/%.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/openjpeg/LICENSE $@
