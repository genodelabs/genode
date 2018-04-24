MIRROR_FROM_REP_DIR := src/app/qt5/qt_avplay \
                       src/app/qt5/tmpl/target_defaults.inc \
                       src/app/qt5/tmpl/target_final.inc

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_OS := include/init/child_policy.h

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/qt5)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
