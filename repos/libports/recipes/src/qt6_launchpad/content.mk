MIRROR_FROM_REP_DIR := src/app/qt6/qt_launchpad

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_DEMO := include/launchpad \
                    lib/mk/launchpad.mk \
                    src/lib/launchpad

content: $(MIRROR_FROM_DEMO)

$(MIRROR_FROM_DEMO):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/demo/$@ $(dir $@)

MIRROR_FROM_OS := include/init/child_policy.h

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
