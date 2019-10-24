MIRROR_FROM_REP_DIR = lib/import/import-mpc.mk lib/symbols/mpc

content: include LICENSE $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mpc)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/mpc/* $@/

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mpc/COPYING.LESSER $@
