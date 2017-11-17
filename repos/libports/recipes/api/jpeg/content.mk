content: include lib/symbols/jpeg LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jpeg)

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/jpeg/* $@/
	cp -r $(REP_DIR)/include/jpeg/* $@/

lib/symbols/jpeg:
	$(mirror_from_rep_dir)

LICENSE:
	grep '^LEGAL ISSUES$$' -A57 $(PORT_DIR)/src/lib/jpeg/README > $@
