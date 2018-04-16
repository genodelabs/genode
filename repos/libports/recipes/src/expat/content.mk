MIRROR_FROM_REP_DIR := lib/mk/expat.mk

content: $(MIRROR_FROM_REP_DIR) src/lib/expat/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/expat/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = expat" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/expat)

MIRROR_FROM_PORT_DIR := src/lib/expat/contrib \

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/expat/contrib/COPYING $@
