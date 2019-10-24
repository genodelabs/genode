MIRROR_FROM_REP_DIR = lib/import/import-mpc.mk lib/mk/mpc.mk

content: LICENSE $(MIRROR_FROM_REP_DIR) src/lib/mpc

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mpc)

src/lib/mpc:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/mpc/* $@
	echo "LIBS = mpc" > $@/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mpc/COPYING.LESSER $@
