FROM_BASE_NOVA := etc include
FROM_BASE      := lib/mk/timeout.mk src/lib/timeout \
                  lib/mk/alarm.mk   src/lib/alarm

content: $(FROM_BASE_NOVA) $(FROM_BASE) LICENSE

$(FROM_BASE_NOVA):
	$(mirror_from_rep_dir)

$(FROM_BASE):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/base/$@ $@

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@
