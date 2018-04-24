MIRROR_FROM_REP_DIR := include/qt5/qpa_nitpicker \
                       lib/import/import-qt5_qpa_nitpicker.mk \
                       lib/import/import-qt5.inc \
                       lib/symbols/qt5_qpa_nitpicker

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

MIRROR_FROM_PORT_DIR := include/QtEglSupport \
                        include/QtEventDispatcherSupport \
                        include/QtFontDatabaseSupport \
                        include/QtInputSupport

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $@
	cp -r $(PORT_DIR)/$@ $(dir $@)

content: LICENSE

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
