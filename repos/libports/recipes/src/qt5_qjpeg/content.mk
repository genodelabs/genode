MIRROR_FROM_REP_DIR := lib/mk/qt5_qjpeg.mk \
                       src/lib/qt5/qtbase/src/plugins/imageformats/jpeg

content: $(MIRROR_FROM_REP_DIR) src/lib/qt5_qjpeg/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

src/lib/qt5_qjpeg/target.mk:
	mkdir -p $(dir $@)
	echo "LIBS = qt5_qjpeg" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := src/lib/qt5/qt5/qtbase/src/plugins/imageformats/jpeg

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $@
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/qt5/qt5/LICENSE.LGPLv3 $@
