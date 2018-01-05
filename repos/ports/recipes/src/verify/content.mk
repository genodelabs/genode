content: src/app/verify src/app/gnupg LICENSE

GNUPG_SRC_DIR := $(call port_dir,$(REP_DIR)/ports/gnupg)/src/app/gnupg

src/app/verify:
	$(mirror_from_rep_dir)

src/app/gnupg:
	mkdir -p $@
	cp -r $(addprefix $(GNUPG_SRC_DIR)/,g10 common kbx) $@

LICENSE:
	cp $(GNUPG_SRC_DIR)/COPYING $@

