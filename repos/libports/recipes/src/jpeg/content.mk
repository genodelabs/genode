content: src/lib/jpeg lib/mk/jpeg.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jpeg)

src/lib/jpeg:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/jpeg $@
	echo "LIBS = jpeg" > $@/target.mk

lib/mk/%.mk:
	$(mirror_from_rep_dir)

LICENSE:
	grep '^LEGAL ISSUES$$' -A57 $(PORT_DIR)/src/lib/jpeg/README > $@
