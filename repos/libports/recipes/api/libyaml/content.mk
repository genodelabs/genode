MIRROR_FROM_REP_DIR := lib/import/import-libyaml.mk lib/mk/libyaml.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libyaml)

MIRROR_FROM_PORT_DIR := src/lib/yaml

content: $(MIRROR_FROM_PORT_DIR)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: include

include:
	cp -r $(PORT_DIR)/include/yaml $@

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/yaml/LICENSE $@
