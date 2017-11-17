content: src/lib/mupdf lib/mk/mupdf.mk lib/mk/mupdf_host_tools.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mupdf)

src/lib/mupdf:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/mupdf $@
	echo "LIBS = mupdf" > $@/target.mk

lib/mk/%.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mupdf/COPYING $@
