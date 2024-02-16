MIRROR_FROM_REP_DIR := src/app/qt5/examples/virtualkeyboard

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qtvirtualkeyboard/examples/virtualkeyboard/basic \
                        src/lib/qt5/qtvirtualkeyboard/.qmake.conf

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/LICENSE.GPLv3 $@

