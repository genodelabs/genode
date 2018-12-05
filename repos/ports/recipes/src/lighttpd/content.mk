content: src/app/lighttpd LICENSE

LIGHTTPD_CONTRIB_DIR := $(call port_dir,$(REP_DIR)/ports/lighttpd)/src/app/lighttpd

src/app/lighttpd:
	mkdir -p $@
	cp -r $(LIGHTTPD_CONTRIB_DIR)/* $@
	$(mirror_from_rep_dir)

LICENSE:
	cp $(LIGHTTPD_CONTRIB_DIR)/COPYING $@

