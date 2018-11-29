PORT_DIR_SOLO5 := $(call port_dir,$(REP_DIR)/ports/solo5)

SRC_DIR = src/lib/solo5

MIRROR_FROM_REP_DIR = \
	include/solo5 \
	lib/import/import-solo5.mk \
	lib/mk/solo5.mk \

content: $(SRC_DIR) $(MIRROR_FROM_REP_DIR)

$(SRC_DIR):
	mkdir -p $@
	cp -r $(PORT_DIR_SOLO5)/$@/* $@/
	cp -r $(PORT_DIR_SOLO5)/include/solo5/* $@/bindings
	cp $(PORT_DIR_SOLO5)/$@/LICENSE .
	echo 'LIBS=solo5' > $@/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)
