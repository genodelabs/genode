MIRROR_FROM_PORT_AND_REP_DIR := src/app/arora

content: $(MIRROR_FROM_PORT_AND_REP_DIR) LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/arora)

$(MIRROR_FROM_PORT_AND_REP_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $(dir $@)
	$(mirror_from_rep_dir)

MIRROR_FROM_LIBPORTS := src/app/qt5/tmpl/target_defaults.inc \
                        src/app/qt5/tmpl/target_final.inc

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $(dir $@)

LICENSE:
	cp $(PORT_DIR)/src/app/arora/LICENSE.GPL3 $@
