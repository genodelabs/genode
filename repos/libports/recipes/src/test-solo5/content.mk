MIRROR_FROM_REP_DIR := src/test/solo5

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/solo5)

content: $(MIRROR_FROM_REP_DIR) src/lib/solo5/tests src/lib/solo5/bindings LICENSE

src/lib/solo5/bindings: $(PORT_DIR)
	mkdir -p $@
	cp $(PORT_DIR)/$@/*.c $@

src/lib/solo5/tests: $(PORT_DIR)
	mkdir -p $@
	cp -r $(PORT_DIR)/$@/test_* $@

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/solo5/LICENSE $@
