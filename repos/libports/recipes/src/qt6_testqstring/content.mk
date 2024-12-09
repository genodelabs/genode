MIRROR_FROM_REP_DIR := src/app/qt6/examples/testqstring

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt6_base)

MIRROR_FROM_PORT_DIR := src/lib/qt6_base/examples/qtestlib/tutorial1

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

LICENSE:
	cp $(PORT_DIR)/src/lib/qt6_base/LICENSES/GPL-3.0-only.txt $@

